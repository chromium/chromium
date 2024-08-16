// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/scanning/mojom/scanning_type_converters.h"
#include "ash/webui/scanning/mojom/scanning.mojom.h"

#include <utility>

#include "base/notreached.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

namespace {

namespace mojo_ipc = ash::scanning::mojom;

using MojomScanResult = ash::scanning::mojom::ScanResult;
using ProtoScanFailureMode = lorgnette::ScanFailureMode;

using MojomColorMode = ash::scanning::mojom::ColorMode;
using ProtoColorMode = lorgnette::ColorMode;

using MojomSourceType = ash::scanning::mojom::SourceType;
using ProtoSourceType = lorgnette::SourceType;

using MojomFileType = ash::scanning::mojom::FileType;
using ProtoImageFormat = lorgnette::ImageFormat;

// The margin allowed when comparing a scannable area dimension to a page size
// dimension. Accounts for differences due to rounding.
constexpr double kMargin = 1;

// POD struct for page size dimensions in mm.
struct PageSize {
  double width;
  double height;
};

// ISO A3: 297 x 420 mm.
constexpr PageSize kIsoA3PageSize = {
    297,
    420,
};

// ISO A4: 210 x 297 mm.
constexpr PageSize kIsoA4PageSize = {
    210,
    297,
};

// ISO B4: 257 x 364 mm.
constexpr PageSize kIsoB4PageSize = {
    257,
    364,
};

// Legal: 215.9 x 355.6 mm.
constexpr PageSize kLegalPageSize = {
    215.9,
    355.6,
};

// NA Letter: 215.9 x 279.4 mm.
constexpr PageSize kNaLetterPageSize = {
    215.9,
    279.4,
};

// Tabloid: 279.4 x 431.8 mm.
constexpr PageSize kTabloidPageSize = {
    279.4,
    431.8,
};

// Returns true if |area| is large enough to support |page_size|.
bool AreaSupportsPageSize(const lorgnette::ScannableArea& area,
                          const PageSize& page_size) {
  return area.width() + kMargin >= page_size.width &&
         area.height() + kMargin >= page_size.height;
}

// Returns the page sizes the given |area| supports.
std::vector<mojo_ipc::PageSize> GetSupportedPageSizes(
    const lorgnette::ScannableArea& area) {
  std::vector<mojo_ipc::PageSize> page_sizes;
  page_sizes.reserve(7);
  page_sizes.push_back(mojo_ipc::PageSize::kMax);
  if (AreaSupportsPageSize(area, kIsoA3PageSize))
    page_sizes.push_back(mojo_ipc::PageSize::kIsoA3);
  if (AreaSupportsPageSize(area, kIsoA4PageSize))
    page_sizes.push_back(mojo_ipc::PageSize::kIsoA4);
  if (AreaSupportsPageSize(area, kIsoB4PageSize))
    page_sizes.push_back(mojo_ipc::PageSize::kIsoB4);
  if (AreaSupportsPageSize(area, kLegalPageSize))
    page_sizes.push_back(mojo_ipc::PageSize::kLegal);
  if (AreaSupportsPageSize(area, kNaLetterPageSize))
    page_sizes.push_back(mojo_ipc::PageSize::kNaLetter);
  if (AreaSupportsPageSize(area, kTabloidPageSize))
    page_sizes.push_back(mojo_ipc::PageSize::kTabloid);

  return page_sizes;
}

// Sets the scan region based on the given |page_size|. If |page_size| is
// PageSize::kMax, the scan resion is left unset, which will cause the scanner
// to scan the entire scannable area.
void SetScanRegion(const mojo_ipc::PageSize page_size,
                   lorgnette::ScanSettings& settings_out) {
  // The default top-left and bottom-right coordinates are (0,0), so only the
  // bottom-right coordinates need to be set.
  lorgnette::ScanRegion region;
  switch (page_size) {
    case mojo_ipc::PageSize::kIsoA3:
      region.set_bottom_right_x(kIsoA3PageSize.width);
      region.set_bottom_right_y(kIsoA3PageSize.height);
      break;
    case mojo_ipc::PageSize::kIsoA4:
      region.set_bottom_right_x(kIsoA4PageSize.width);
      region.set_bottom_right_y(kIsoA4PageSize.height);
      break;
    case mojo_ipc::PageSize::kIsoB4:
      region.set_bottom_right_x(kIsoB4PageSize.width);
      region.set_bottom_right_y(kIsoB4PageSize.height);
      break;
    case mojo_ipc::PageSize::kLegal:
      region.set_bottom_right_x(kLegalPageSize.width);
      region.set_bottom_right_y(kLegalPageSize.height);
      break;
    case mojo_ipc::PageSize::kNaLetter:
      region.set_bottom_right_x(kNaLetterPageSize.width);
      region.set_bottom_right_y(kNaLetterPageSize.height);
      break;
    case mojo_ipc::PageSize::kTabloid:
      region.set_bottom_right_x(kTabloidPageSize.width);
      region.set_bottom_right_y(kTabloidPageSize.height);
      break;
    case mojo_ipc::PageSize::kMax:
      return;
  }

  *settings_out.mutable_scan_region() = std::move(region);
}

}  // namespace

// static
MojomColorMode EnumTraits<MojomColorMode, ProtoColorMode>::ToMojom(
    ProtoColorMode input) {
  switch (input) {
    case ProtoColorMode::MODE_LINEART:
      return MojomColorMode::kBlackAndWhite;
    case ProtoColorMode::MODE_GRAYSCALE:
      return MojomColorMode::kGrayscale;
    case ProtoColorMode::MODE_COLOR:
      return MojomColorMode::kColor;
    case ProtoColorMode::MODE_UNSPECIFIED:
    case ProtoColorMode::ColorMode_INT_MIN_SENTINEL_DO_NOT_USE_:
    case ProtoColorMode::ColorMode_INT_MAX_SENTINEL_DO_NOT_USE_:
      NOTREACHED();
  }
}

// static
bool EnumTraits<MojomColorMode, ProtoColorMode>::FromMojom(
    MojomColorMode input,
    ProtoColorMode* out) {
  switch (input) {
    case MojomColorMode::kBlackAndWhite:
      *out = ProtoColorMode::MODE_LINEART;
      return true;
    case MojomColorMode::kGrayscale:
      *out = ProtoColorMode::MODE_GRAYSCALE;
      return true;
    case MojomColorMode::kColor:
      *out = ProtoColorMode::MODE_COLOR;
      return true;
  }
  NOTREACHED();
}

// static
MojomSourceType EnumTraits<MojomSourceType, ProtoSourceType>::ToMojom(
    ProtoSourceType input) {
  switch (input) {
    case ProtoSourceType::SOURCE_PLATEN:
      return MojomSourceType::kFlatbed;
    case ProtoSourceType::SOURCE_ADF_SIMPLEX:
      return MojomSourceType::kAdfSimplex;
    case ProtoSourceType::SOURCE_ADF_DUPLEX:
      return MojomSourceType::kAdfDuplex;
    case ProtoSourceType::SOURCE_DEFAULT:
      return MojomSourceType::kDefault;
    case ProtoSourceType::SOURCE_UNSPECIFIED:
      return MojomSourceType::kUnknown;
    case ProtoSourceType::SourceType_INT_MIN_SENTINEL_DO_NOT_USE_:
    case ProtoSourceType::SourceType_INT_MAX_SENTINEL_DO_NOT_USE_:
      NOTREACHED();
  }
}

// static
bool EnumTraits<MojomSourceType, ProtoSourceType>::FromMojom(
    MojomSourceType input,
    ProtoSourceType* out) {
  switch (input) {
    case MojomSourceType::kFlatbed:
      *out = ProtoSourceType::SOURCE_PLATEN;
      return true;
    case MojomSourceType::kAdfSimplex:
      *out = ProtoSourceType::SOURCE_ADF_SIMPLEX;
      return true;
    case MojomSourceType::kAdfDuplex:
      *out = ProtoSourceType::SOURCE_ADF_DUPLEX;
      return true;
    case MojomSourceType::kDefault:
      *out = ProtoSourceType::SOURCE_DEFAULT;
      return true;
    case MojomSourceType::kUnknown:
      *out = ProtoSourceType::SOURCE_UNSPECIFIED;
      return true;
  }
  NOTREACHED();
}

// static
MojomFileType EnumTraits<MojomFileType, ProtoImageFormat>::ToMojom(
    ProtoImageFormat input) {
  switch (input) {
    case ProtoImageFormat::IMAGE_FORMAT_PNG:
      return MojomFileType::kPng;
    case ProtoImageFormat::IMAGE_FORMAT_JPEG:
      return MojomFileType::kJpg;
    case ProtoImageFormat::ImageFormat_INT_MIN_SENTINEL_DO_NOT_USE_:
    case ProtoImageFormat::ImageFormat_INT_MAX_SENTINEL_DO_NOT_USE_:
      NOTREACHED();
  }
}

// static
bool EnumTraits<MojomFileType, ProtoImageFormat>::FromMojom(
    MojomFileType input,
    ProtoImageFormat* out) {
  switch (input) {
    case MojomFileType::kPng:
      *out = ProtoImageFormat::IMAGE_FORMAT_PNG;
      return true;
    // PDF images request JPEG data from lorgnette, then
    // convert the returned JPEG data to PDF.
    case MojomFileType::kPdf:  // FALLTHROUGH
    case MojomFileType::kJpg:
      *out = ProtoImageFormat::IMAGE_FORMAT_JPEG;
      return true;
  }
  NOTREACHED();
}

// static
MojomScanResult EnumTraits<MojomScanResult, ProtoScanFailureMode>::ToMojom(
    ProtoScanFailureMode input) {
  switch (input) {
    case ProtoScanFailureMode::SCAN_FAILURE_MODE_NO_FAILURE:
      return MojomScanResult::kSuccess;
    case ProtoScanFailureMode::SCAN_FAILURE_MODE_UNKNOWN:
      return MojomScanResult::kUnknownError;
    case ProtoScanFailureMode::SCAN_FAILURE_MODE_DEVICE_BUSY:
      return MojomScanResult::kDeviceBusy;
    case ProtoScanFailureMode::SCAN_FAILURE_MODE_ADF_JAMMED:
      return MojomScanResult::kAdfJammed;
    case ProtoScanFailureMode::SCAN_FAILURE_MODE_ADF_EMPTY:
      return MojomScanResult::kAdfEmpty;
    case ProtoScanFailureMode::SCAN_FAILURE_MODE_FLATBED_OPEN:
      return MojomScanResult::kFlatbedOpen;
    case ProtoScanFailureMode::SCAN_FAILURE_MODE_IO_ERROR:
      return MojomScanResult::kIoError;
    case ProtoScanFailureMode::ScanFailureMode_INT_MIN_SENTINEL_DO_NOT_USE_:
    case ProtoScanFailureMode::ScanFailureMode_INT_MAX_SENTINEL_DO_NOT_USE_:
      break;
  }
  NOTREACHED();
}

// static
bool EnumTraits<MojomScanResult, ProtoScanFailureMode>::FromMojom(
    MojomScanResult input,
    ProtoScanFailureMode* output) {
  switch (input) {
    case MojomScanResult::kSuccess:
      *output = ProtoScanFailureMode::SCAN_FAILURE_MODE_NO_FAILURE;
      return true;
    case MojomScanResult::kUnknownError:
      *output = ProtoScanFailureMode::SCAN_FAILURE_MODE_UNKNOWN;
      return true;
    case MojomScanResult::kDeviceBusy:
      *output = ProtoScanFailureMode::SCAN_FAILURE_MODE_DEVICE_BUSY;
      return true;
    case MojomScanResult::kAdfJammed:
      *output = ProtoScanFailureMode::SCAN_FAILURE_MODE_ADF_JAMMED;
      return true;
    case MojomScanResult::kAdfEmpty:
      *output = ProtoScanFailureMode::SCAN_FAILURE_MODE_ADF_EMPTY;
      return true;
    case MojomScanResult::kFlatbedOpen:
      *output = ProtoScanFailureMode::SCAN_FAILURE_MODE_FLATBED_OPEN;
      return true;
    case MojomScanResult::kIoError:
      *output = ProtoScanFailureMode::SCAN_FAILURE_MODE_IO_ERROR;
      return true;
  }
  NOTREACHED();
}

// static
mojo_ipc::ScannerCapabilitiesPtr
StructTraits<ash::scanning::mojom::ScannerCapabilitiesPtr,
             lorgnette::ScannerCapabilities>::
    ToMojom(const lorgnette::ScannerCapabilities& lorgnette_caps) {
  mojo_ipc::ScannerCapabilities mojo_caps;
  mojo_caps.sources.reserve(lorgnette_caps.sources().size());
  for (const auto& source : lorgnette_caps.sources()) {
    mojo_ipc::ScanSourcePtr mojo_source = mojo_ipc::ScanSource::New();
    mojo_source->type =
        mojo::EnumTraits<mojo_ipc::SourceType, lorgnette::SourceType>::ToMojom(
            source.type());
    mojo_source->name = source.name();
    mojo_source->page_sizes = GetSupportedPageSizes(source.area());

    mojo_source->color_modes.reserve(source.color_modes().size());
    for (const auto& mode : source.color_modes()) {
      mojo_source->color_modes.push_back(
          mojo::EnumTraits<mojo_ipc::ColorMode, lorgnette::ColorMode>::ToMojom(
              static_cast<lorgnette::ColorMode>(mode)));
    }

    mojo_source->resolutions.reserve(source.resolutions().size());
    for (const auto& res : source.resolutions())
      mojo_source->resolutions.push_back(res);

    mojo_caps.sources.push_back(std::move(mojo_source));
  }

  return mojo_caps.Clone();
}

// static
lorgnette::ScanSettings
StructTraits<lorgnette::ScanSettings, mojo_ipc::ScanSettingsPtr>::ToMojom(
    const mojo_ipc::ScanSettingsPtr& mojo_settings) {
  lorgnette::ScanSettings lorgnette_settings;
  lorgnette::ColorMode lorgnette_color_mode;
  lorgnette::ImageFormat lorgnette_image_format;

  if (mojo::EnumTraits<mojo_ipc::ColorMode, lorgnette::ColorMode>::FromMojom(
          mojo_settings->color_mode, &lorgnette_color_mode)) {
    lorgnette_settings.set_color_mode(lorgnette_color_mode);
  }
  if (mojo::EnumTraits<mojo_ipc::FileType, lorgnette::ImageFormat>::FromMojom(
          mojo_settings->file_type, &lorgnette_image_format)) {
    lorgnette_settings.set_image_format(lorgnette_image_format);
  }

  lorgnette_settings.set_source_name(mojo_settings->source_name);
  lorgnette_settings.set_resolution(mojo_settings->resolution_dpi);
  SetScanRegion(mojo_settings->page_size, lorgnette_settings);
  return lorgnette_settings;
}

}  // namespace mojo
