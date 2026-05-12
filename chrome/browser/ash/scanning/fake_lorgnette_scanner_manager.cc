// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scanning/fake_lorgnette_scanner_manager.h"

#include <algorithm>
#include <initializer_list>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/map_util.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/string_view_util.h"
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
  std::optional<std::vector<uint8_t>> bytes =
      gfx::JPEGCodec::Encode(bitmap, /*quality=*/90);
  return std::string(base::as_string_view(bytes.value()));
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

lorgnette::ScannerCapabilities CreateDefaultCapabilities() {
  lorgnette::ScannerCapabilities caps;
  lorgnette::DocumentSource* source = caps.add_sources();
  source->set_type(lorgnette::SOURCE_PLATEN);
  source->set_name("Flatbed");
  source->add_color_modes(lorgnette::MODE_COLOR);
  source->add_resolutions(75);
  source->add_resolutions(300);
  return caps;
}

}  // namespace

FakeLorgnetteScannerManager::FakeLorgnetteScannerManager() = default;

FakeLorgnetteScannerManager::~FakeLorgnetteScannerManager() = default;

FakeLorgnetteScannerManager::ScannerSession::ScannerSession() = default;
FakeLorgnetteScannerManager::ScannerSession::ScannerSession(
    ScannerSession&& other) noexcept = default;
FakeLorgnetteScannerManager::ScannerSession&
FakeLorgnetteScannerManager::ScannerSession::operator=(
    ScannerSession&& other) noexcept = default;
FakeLorgnetteScannerManager::ScannerSession::~ScannerSession() = default;

FakeLorgnetteScannerManager::ScannerState::ScannerState(
    lorgnette::ScannerInfo info,
    lorgnette::ScannerConfig template_config,
    lorgnette::ScannerCapabilities capabilities)
    : info(std::move(info)),
      template_config(std::move(template_config)),
      capabilities(std::move(capabilities)) {}

FakeLorgnetteScannerManager::ScannerState::ScannerState(
    ScannerState&& other) noexcept = default;
FakeLorgnetteScannerManager::ScannerState&
FakeLorgnetteScannerManager::ScannerState::operator=(
    ScannerState&& other) noexcept = default;
FakeLorgnetteScannerManager::ScannerState::~ScannerState() = default;

void FakeLorgnetteScannerManager::GetScannerNames(
    GetScannerNamesCallback callback) {
  std::vector<std::string> names = base::ToVector(
      scanners_, [](const ScannerState& state) { return state.info.name(); });
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(names)));
}

void FakeLorgnetteScannerManager::GetScannerInfoList(
    const std::string& client_id,
    LocalScannerFilter local_only,
    SecureScannerFilter secure_only,
    GetScannerInfoListCallback callback) {
  lorgnette::ListScannersResponse response;
  for (const ScannerState& state : scanners_) {
    *response.add_scanners() = state.info;
  }
  response.set_result(lorgnette::OPERATION_RESULT_SUCCESS);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(response)));
}

void FakeLorgnetteScannerManager::GetScannerCapabilities(
    const std::string& scanner_name,
    GetScannerCapabilitiesCallback callback) {
  if (simulate_dbus_failure_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  auto it = std::ranges::find_if(scanners_, [&scanner_name](const auto& state) {
    return state.info.name() == scanner_name;
  });

  std::optional<lorgnette::ScannerCapabilities> caps;
  if (it != scanners_.end()) {
    caps = it->capabilities;
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(caps)));
}

void FakeLorgnetteScannerManager::OpenScanner(
    const lorgnette::OpenScannerRequest& request,
    OpenScannerCallback callback) {
  CHECK(request.has_scanner_id());

  if (simulate_dbus_failure_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  lorgnette::OpenScannerResponse response;
  *response.mutable_scanner_id() = request.scanner_id();

  const std::string& scanner_id = request.scanner_id().connection_string();
  auto it = std::ranges::find_if(scanners_, [&scanner_id](const auto& state) {
    return state.info.name() == scanner_id;
  });

  if (it == scanners_.end()) {
    response.set_result(lorgnette::OPERATION_RESULT_MISSING);
  } else if (it->active_session.has_value() &&
             it->active_session->client_id != request.client_id()) {
    response.set_result(lorgnette::OPERATION_RESULT_DEVICE_BUSY);
  } else {
    response.set_result(lorgnette::OPERATION_RESULT_SUCCESS);
    response.mutable_config()->CopyFrom(it->template_config);
    std::string scanner_handle = CreateFreshHandle();
    response.mutable_config()->mutable_scanner()->set_token(scanner_handle);
    it->active_session.emplace();
    it->active_session->client_id = request.client_id();
    it->active_session->config = response.config();
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(response)));
}

void FakeLorgnetteScannerManager::CloseScanner(
    const lorgnette::CloseScannerRequest& request,
    CloseScannerCallback callback) {
  CHECK(request.has_scanner());

  if (simulate_dbus_failure_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  lorgnette::CloseScannerResponse response;
  response.set_result(lorgnette::OPERATION_RESULT_INVALID);
  *response.mutable_scanner() = request.scanner();
  const std::string& handle = request.scanner().token();
  auto it = std::ranges::find_if(scanners_, [&handle](const auto& s) {
    return s.active_session.has_value() &&
           s.active_session->config.scanner().token() == handle;
  });
  if (it != scanners_.end()) {
    it->active_session.reset();
    response.set_result(lorgnette::OPERATION_RESULT_SUCCESS);
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(response)));
}

void FakeLorgnetteScannerManager::SetOptions(
    const lorgnette::SetOptionsRequest& request,
    SetOptionsCallback callback) {
  CHECK(request.has_scanner());

  if (simulate_dbus_failure_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  lorgnette::SetOptionsResponse response;
  *response.mutable_scanner() = request.scanner();

  const std::string& scanner_handle = request.scanner().token();
  lorgnette::ScannerConfig* config = nullptr;
  for (auto& s : scanners_) {
    if (s.active_session.has_value() &&
        s.active_session->config.scanner().token() == scanner_handle) {
      config = &s.active_session->config;
      break;
    }
  }
  if (!config) {
    for (const lorgnette::ScannerOption& setting : request.options()) {
      (*response.mutable_results())[setting.name()] =
          lorgnette::OPERATION_RESULT_INVALID;
    }
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(response)));
    return;
  }

  for (const lorgnette::ScannerOption& setting : request.options()) {
    lorgnette::OperationResult result = lorgnette::OPERATION_RESULT_SUCCESS;
    lorgnette::ScannerOption* option =
        &(*config->mutable_options())[setting.name()];
    if (option->name().empty()) {
      // This entry was just newly inserted.
      option->set_name(setting.name());
      option->set_option_type(setting.option_type());
    }

    // Make sure type and value match.
    switch (setting.value_case()) {
      case lorgnette::ScannerOption::kBoolValue:
        if (option->option_type() == lorgnette::TYPE_BOOL) {
          option->set_bool_value(setting.bool_value());
        } else {
          result = lorgnette::OPERATION_RESULT_WRONG_TYPE;
        }
        break;
      case lorgnette::ScannerOption::kIntValue:
        if (option->option_type() == lorgnette::TYPE_INT) {
          *option->mutable_int_value() = setting.int_value();
        } else {
          result = lorgnette::OPERATION_RESULT_WRONG_TYPE;
        }
        break;
      case lorgnette::ScannerOption::kFixedValue:
        if (option->option_type() == lorgnette::TYPE_FIXED) {
          *option->mutable_fixed_value() = setting.fixed_value();
        } else {
          result = lorgnette::OPERATION_RESULT_WRONG_TYPE;
        }
        break;
      case lorgnette::ScannerOption::kStringValue:
        if (option->option_type() == lorgnette::TYPE_STRING) {
          option->set_string_value(setting.string_value());
        } else {
          result = lorgnette::OPERATION_RESULT_WRONG_TYPE;
        }
        break;
      case lorgnette::ScannerOption::VALUE_NOT_SET:
        // No value specified, it will be auto-set.
        break;
      default:
        NOTREACHED();
    }

    (*response.mutable_results())[setting.name()] = result;
  }

  response.mutable_config()->CopyFrom(*config);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(response)));
}

void FakeLorgnetteScannerManager::GetCurrentConfig(
    const lorgnette::GetCurrentConfigRequest& request,
    GetCurrentConfigCallback callback) {
  CHECK(request.has_scanner());
  std::optional<lorgnette::GetCurrentConfigResponse> response;
  if (get_current_config_result_.has_value()) {
    response.emplace();
    *response->mutable_scanner() = request.scanner();
    response->set_result(*get_current_config_result_);
    if (get_current_config_config_.has_value()) {
      *response->mutable_config() = *get_current_config_config_;
    }
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(response)));
}

void FakeLorgnetteScannerManager::StartPreparedScan(
    const lorgnette::StartPreparedScanRequest& request,
    StartPreparedScanCallback callback) {
  CHECK(request.has_scanner());
  std::optional<lorgnette::StartPreparedScanResponse> response;
  if (start_prepared_scan_result_.has_value()) {
    response.emplace();
    *response->mutable_scanner() = request.scanner();
    if (request.has_max_read_size() && request.max_read_size() < 32768) {
      response->set_result(lorgnette::OPERATION_RESULT_INVALID);
    } else {
      response->set_result(*start_prepared_scan_result_);
      if (response->result() == lorgnette::OPERATION_RESULT_SUCCESS) {
        response->mutable_job_handle()->set_token(CreateFreshHandle());
      }
    }
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(response)));
}

void FakeLorgnetteScannerManager::ReadScanData(
    const lorgnette::ReadScanDataRequest& request,
    ReadScanDataCallback callback) {
  CHECK(request.has_job_handle());
  std::optional<lorgnette::ReadScanDataResponse> response;
  if (read_scan_data_result_.has_value()) {
    response.emplace();
    response->mutable_job_handle()->set_token(request.job_handle().token());
    if (std::find(cancelled_jobs_.begin(), cancelled_jobs_.end(),
                  request.job_handle().token()) != cancelled_jobs_.end()) {
      response->set_result(lorgnette::OPERATION_RESULT_CANCELLED);
    } else if (!read_scan_data_chunks_.empty()) {
      response->set_result(lorgnette::OPERATION_RESULT_SUCCESS);
      response->set_data(read_scan_data_chunks_[0]);
      read_scan_data_chunks_.erase(read_scan_data_chunks_.begin());
      response->set_estimated_completion(
          read_scan_data_chunks_.empty()
              ? 100
              : 100 / (read_scan_data_chunks_.size() + 1));
    } else {
      response->set_result(*read_scan_data_result_);
    }
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(response)));
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
  if (simulate_dbus_failure_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  lorgnette::CancelScanResponse response;
  response.set_success(false);

  if (request.has_job_handle()) {
    *response.mutable_job_handle() = request.job_handle();
    const std::string& job_handle = request.job_handle().token();
    if (std::find(cancelled_jobs_.begin(), cancelled_jobs_.end(), job_handle) ==
        cancelled_jobs_.end()) {
      response.set_success(true);
      response.set_result(lorgnette::OPERATION_RESULT_SUCCESS);
      cancelled_jobs_.push_back(job_handle);
    } else {
      response.set_result(lorgnette::OPERATION_RESULT_UNKNOWN);
    }
  } else {
    response.set_result(lorgnette::OPERATION_RESULT_INVALID);
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(response)));
}

void FakeLorgnetteScannerManager::SimulateDBusFailure(bool simulate) {
  simulate_dbus_failure_ = simulate;
}

void FakeLorgnetteScannerManager::AddScanner(
    lorgnette::ScannerInfo scanner_info,
    lorgnette::ScannerConfig config_template,
    std::optional<lorgnette::ScannerCapabilities> capabilities) {
  CHECK(std::ranges::none_of(scanners_, [&scanner_info](const auto& state) {
    return state.info.name() == scanner_info.name();
  }));
  scanners_.emplace_back(std::move(scanner_info), std::move(config_template),
                         capabilities.has_value()
                             ? std::move(*capabilities)
                             : CreateDefaultCapabilities());
}

void FakeLorgnetteScannerManager::ConfigureGetCurrentConfigResponse(
    std::optional<lorgnette::OperationResult> result,
    std::optional<lorgnette::ScannerConfig> config) {
  get_current_config_result_ = std::move(result);
  get_current_config_config_ = std::move(config);
}

void FakeLorgnetteScannerManager::SetStartPreparedScanResult(
    std::optional<lorgnette::OperationResult> result) {
  start_prepared_scan_result_ = std::move(result);
}

void FakeLorgnetteScannerManager::ConfigureReadScanDataResponse(
    std::optional<lorgnette::OperationResult> result,
    std::vector<std::string> data_chunks) {
  read_scan_data_result_ = std::move(result);
  read_scan_data_chunks_ = std::move(data_chunks);
}

void FakeLorgnetteScannerManager::SetScanResponse(
    const std::optional<std::vector<std::string>>& scan_data) {
  scan_data_ = scan_data;
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

std::string FakeLorgnetteScannerManager::CreateFreshHandle() {
  return base::StrCat({"handle-", base::NumberToString(handle_count_++)});
}

}  // namespace ash
