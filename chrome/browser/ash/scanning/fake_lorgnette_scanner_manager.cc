// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scanning/fake_lorgnette_scanner_manager.h"

#include <initializer_list>
#include <string_view>
#include <utility>

#include "base/containers/fixed_flat_map.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/ash/components/dbus/lorgnette/lorgnette_service.pb.h"
#include "third_party/re2/src/re2/re2.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"

namespace ash {

namespace {

using ProtoScanFailureMode = lorgnette::ScanFailureMode;
using ProtoColorMode = lorgnette::ColorMode;
using ProtoSourceType = lorgnette::SourceType;
using ProtoImageFormat = lorgnette::ImageFormat;
using ProtoScanRegion = lorgnette::ScanRegion;

std::string GetColorModeString(ProtoColorMode color_mode) {
  switch (color_mode) {
    case ProtoColorMode::MODE_GRAYSCALE:
      return "grayscale";
    case lorgnette::MODE_COLOR:
      return "color";
    case lorgnette::MODE_LINEART:
      return "black_and_white";
    case lorgnette::MODE_UNSPECIFIED:
    case ProtoColorMode::ColorMode_INT_MIN_SENTINEL_DO_NOT_USE_:
    case ProtoColorMode::ColorMode_INT_MAX_SENTINEL_DO_NOT_USE_:
      NOTREACHED();
  }
}

static constexpr auto kPageSizeToPageSizeStrMap =
    base::MakeFixedFlatMap<std::pair<double, double>, std::string_view>({
        {{297, 420}, "a3"},           // ISO A3: 297 x 420 mm
        {{210, 297}, "a4"},           // ISO A4: 210 x 297 mm.
        {{257, 364}, "b4"},           // ISO B4: 257 x 364 mm.
        {{215.9, 355.6}, "legal"},    // Legal: 215.9 x 355.6 mm.
        {{215.9, 279.4}, "letter"},   // NA Letter: 215.9 x 279.4 mm.
        {{279.4, 431.8}, "tabloid"},  // Tabloid: 279.4 x 431.8 mm.
        {{0, 0}, "max"},              // Max: the scan region is left unset.
    });

std::string GetPageSizeString(const ProtoScanRegion& scan_region) {
  auto bottom_right_x = scan_region.bottom_right_x();
  auto bottom_right_y = scan_region.bottom_right_y();
  const auto bottom_region = std::make_pair(bottom_right_x, bottom_right_y);
  for (const auto& entry : kPageSizeToPageSizeStrMap) {
    if (bottom_region == entry.first) {
      return std::string(entry.second);
    }
  }

  NOTREACHED();
}

std::string GetImageFormatString(ProtoImageFormat img_format) {
  switch (img_format) {
    case lorgnette::IMAGE_FORMAT_PNG:
      return "png";
    case lorgnette::IMAGE_FORMAT_JPEG:
      return "jpeg";
    case lorgnette::ImageFormat_INT_MIN_SENTINEL_DO_NOT_USE_:
    case lorgnette::ImageFormat_INT_MAX_SENTINEL_DO_NOT_USE_:
      NOTREACHED();
  }
}

std::string GetResolution(uint32_t resolution) {
  return base::StrCat({base::NumberToString(resolution), "_dpi"});
}

std::string GetScanSettingsMapKey(const lorgnette::ScanSettings& settings) {
  std::initializer_list<std::string> parts = {
      base::ToLowerASCII(settings.source_name()),
      GetImageFormatString(settings.image_format()),
      GetColorModeString(settings.color_mode()),
      GetPageSizeString(settings.scan_region()),
      GetResolution(settings.resolution())};
  return base::JoinString(parts, "_");
}

// Maps a specific `ScanSettings` combination to an `alpha` which will be used
// by `CreateJpeg` to generate a JPEG image. The generated JPEG image will
// be used to validate that a set of scan settings will always produce the
// same output.
static constexpr auto kScanSettingsToAlphaMap =
    base::MakeFixedFlatMap<std::string_view, int>(
        {{"flatbed_jpeg_color_letter_300_dpi", /*alpha=*/1},
         {"adf_simplex_jpeg_grayscale_max_150_dpi", /*alpha=*/2},
         {"flatbed_jpeg_grayscale_max_150_dpi", /*alpha=*/3}});

// Returns a manually generated JPEG image. `alpha` is used to make them unique.
std::string CreateJpeg(const int alpha = 255) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(100, 100);
  bitmap.eraseARGB(alpha, 0, 0, 255);
  std::vector<unsigned char> bytes;
  CHECK(gfx::JPEGCodec::Encode(bitmap, 90, &bytes));
  return std::string(bytes.begin(), bytes.end());
}

// A list of Epson models that do not rotate alternating ADF scanned pages
// to be excluded in IsRotateAlternate().
constexpr char kEpsonNoFlipModels[] =
    "\\b("
    "DS-790WN"
    "|LP-M8180A"
    "|LP-M8180F"
    "|LX-10020M"
    "|LX-10050KF"
    "|LX-10050MF"
    "|LX-6050MF"
    "|LX-7550MF"
    "|PX-M7070FX"
    "|PX-M7080FX"
    "|PX-M7090FX"
    "|PX-M7110F"
    "|PX-M7110FP"
    "|PX-M860F"
    "|PX-M880FX"
    "|WF-6530"
    "|WF-6590"
    "|WF-6593"
    "|WF-C20600"
    "|WF-C20600a"
    "|WF-C20600c"
    "|WF-C20750"
    "|WF-C20750a"
    "|WF-C20750c"
    "|WF-C21000"
    "|WF-C21000a"
    "|WF-C21000c"
    "|WF-C579R"
    "|WF-C579Ra"
    "|WF-C8610"
    "|WF-C8690"
    "|WF-C8690a"
    "|WF-C869R"
    "|WF-C869Ra"
    "|WF-C878R"
    "|WF-C878Ra"
    "|WF-C879R"
    "|WF-C879Ra"
    "|WF-M21000"
    "|WF-M21000a"
    "|WF-M21000c"
    ")\\b";

}  // namespace

FakeLorgnetteScannerManager::FakeLorgnetteScannerManager() = default;

FakeLorgnetteScannerManager::~FakeLorgnetteScannerManager() = default;

void FakeLorgnetteScannerManager::GetScannerNames(
    GetScannerNamesCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), scanner_names_));
}

void FakeLorgnetteScannerManager::GetScannerInfoList(
    const std::string& client_id,
    LocalScannerFilter local_only,
    SecureScannerFilter secure_only,
    GetScannerInfoListCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), list_scanners_response_));
}

void FakeLorgnetteScannerManager::GetScannerCapabilities(
    const std::string& scanner_name,
    GetScannerCapabilitiesCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), scanner_capabilities_));
}

void FakeLorgnetteScannerManager::OpenScanner(
    const lorgnette::OpenScannerRequest& request,
    OpenScannerCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), open_scanner_response_));
}

void FakeLorgnetteScannerManager::CloseScanner(
    const lorgnette::CloseScannerRequest& request,
    CloseScannerCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), close_scanner_response_));
}

void FakeLorgnetteScannerManager::SetOptions(
    const lorgnette::SetOptionsRequest& request,
    SetOptionsCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), set_options_response_));
}

void FakeLorgnetteScannerManager::GetCurrentConfig(
    const lorgnette::GetCurrentConfigRequest& request,
    GetCurrentConfigCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), get_current_config_response_));
}

void FakeLorgnetteScannerManager::StartPreparedScan(
    const lorgnette::StartPreparedScanRequest& request,
    StartPreparedScanCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), start_prepared_scan_response_));
}

void FakeLorgnetteScannerManager::ReadScanData(
    const lorgnette::ReadScanDataRequest& request,
    ReadScanDataCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), read_scan_data_response_));
}

bool FakeLorgnetteScannerManager::IsRotateAlternate(
    const std::string& scanner_name,
    const std::string& source_name) {
  if (!RE2::PartialMatch(source_name, RE2("(?i)adf duplex"))) {
    return false;
  }

  // No implementation of GetUsableDeviceNameAndProtocol() available
  // so assume scanner name is formatted as device_name.
  std::string exclude_regex = std::string("^(airscan|ippusb).*(EPSON\\s+)?") +
                              std::string(kEpsonNoFlipModels);
  if (RE2::PartialMatch(scanner_name, RE2("^(epsonds|epson2)")) ||
      RE2::PartialMatch(scanner_name, RE2(exclude_regex))) {
    return false;
  }

  return RE2::PartialMatch(scanner_name, RE2("(?i)epson"));
}

void FakeLorgnetteScannerManager::Scan(const std::string& scanner_name,
                                       const lorgnette::ScanSettings& settings,
                                       ProgressCallback progress_callback,
                                       PageCallback page_callback,
                                       CompletionCallback completion_callback) {
  MaybeSetScanDataBasedOnSettings(settings);
  if (scan_data_.has_value()) {
    uint32_t page_number = 1;
    for (const std::string& page_data : scan_data_.value()) {
      if (progress_callback) {
        for (const uint32_t progress : {7, 22, 40, 42, 59, 74, 95}) {
          base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE,
              base::BindOnce(progress_callback, progress, page_number));
        }
      }

      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(page_callback, page_data, page_number++));
    }
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(completion_callback),
                     scan_data_.has_value()
                         ? lorgnette::SCAN_FAILURE_MODE_NO_FAILURE
                         : lorgnette::SCAN_FAILURE_MODE_DEVICE_BUSY));
}

void FakeLorgnetteScannerManager::CancelScan(CancelCallback cancel_callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(cancel_callback), true));
}

void FakeLorgnetteScannerManager::CancelScan(
    const lorgnette::CancelScanRequest& request,
    CancelScanCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), cancel_scan_response_));
}

void FakeLorgnetteScannerManager::SetGetScannerNamesResponse(
    const std::vector<std::string>& scanner_names) {
  scanner_names_ = scanner_names;
}

void FakeLorgnetteScannerManager::SetGetScannerInfoListResponse(
    const std::optional<lorgnette::ListScannersResponse>& response) {
  list_scanners_response_ = response;
}

void FakeLorgnetteScannerManager::SetGetScannerCapabilitiesResponse(
    const std::optional<lorgnette::ScannerCapabilities>& scanner_capabilities) {
  scanner_capabilities_ = scanner_capabilities;
}

void FakeLorgnetteScannerManager::SetOpenScannerResponse(
    const std::optional<lorgnette::OpenScannerResponse>& response) {
  open_scanner_response_ = response;
}

void FakeLorgnetteScannerManager::SetCloseScannerResponse(
    const std::optional<lorgnette::CloseScannerResponse>& response) {
  close_scanner_response_ = response;
}

void FakeLorgnetteScannerManager::SetSetOptionsResponse(
    const std::optional<lorgnette::SetOptionsResponse>& response) {
  set_options_response_ = response;
}

void FakeLorgnetteScannerManager::SetGetCurrentConfigResponse(
    const std::optional<lorgnette::GetCurrentConfigResponse>& response) {
  get_current_config_response_ = response;
}

void FakeLorgnetteScannerManager::SetStartPreparedScanResponse(
    const std::optional<lorgnette::StartPreparedScanResponse>& response) {
  start_prepared_scan_response_ = response;
}

void FakeLorgnetteScannerManager::SetReadScanDataResponse(
    const std::optional<lorgnette::ReadScanDataResponse>& response) {
  read_scan_data_response_ = response;
}

void FakeLorgnetteScannerManager::SetScanResponse(
    const std::optional<std::vector<std::string>>& scan_data) {
  scan_data_ = scan_data;
}

void FakeLorgnetteScannerManager::SetCancelScanResponse(
    const std::optional<lorgnette::CancelScanResponse>& response) {
  cancel_scan_response_ = response;
}

void FakeLorgnetteScannerManager::MaybeSetScanDataBasedOnSettings(
    const lorgnette::ScanSettings& settings) {
  const auto match =
      kScanSettingsToAlphaMap.find(GetScanSettingsMapKey(settings));
  if (match != kScanSettingsToAlphaMap.end()) {
    SetScanResponse(
        std::initializer_list<std::string>{CreateJpeg(match->second)});
  }
}

}  // namespace ash
