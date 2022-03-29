// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scanning/scanning_type_converters.h"

#include <utility>

#include "base/notreached.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

namespace {

namespace mojo_ipc = ash::scanning::mojom;

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

template <>
struct TypeConverter<mojo_ipc::ColorMode, lorgnette::ColorMode> {
  static mojo_ipc::ColorMode Convert(lorgnette::ColorMode mode) {
    switch (mode) {
      case lorgnette::MODE_LINEART:
        return mojo_ipc::ColorMode::kBlackAndWhite;
      case lorgnette::MODE_GRAYSCALE:
        return mojo_ipc::ColorMode::kGrayscale;
      case lorgnette::MODE_COLOR:
        return mojo_ipc::ColorMode::kColor;
      case lorgnette::MODE_UNSPECIFIED:
      case lorgnette::ColorMode_INT_MIN_SENTINEL_DO_NOT_USE_:
      case lorgnette::ColorMode_INT_MAX_SENTINEL_DO_NOT_USE_:
        NOTREACHED();
        return mojo_ipc::ColorMode::kColor;
    }
  }
};

template <>
struct TypeConverter<mojo_ipc::SourceType, lorgnette::SourceType> {
  static mojo_ipc::SourceType Convert(lorgnette::SourceType type) {
    switch (type) {
      case lorgnette::SOURCE_PLATEN:
        return mojo_ipc::SourceType::kFlatbed;
      case lorgnette::SOURCE_ADF_SIMPLEX:
        return mojo_ipc::SourceType::kAdfSimplex;
      case lorgnette::SOURCE_ADF_DUPLEX:
        return mojo_ipc::SourceType::kAdfDuplex;
      case lorgnette::SOURCE_DEFAULT:
        return mojo_ipc::SourceType::kDefault;
      case lorgnette::SOURCE_UNSPECIFIED:
      case lorgnette::SourceType_INT_MIN_SENTINEL_DO_NOT_USE_:
      case lorgnette::SourceType_INT_MAX_SENTINEL_DO_NOT_USE_:
        NOTREACHED();
        return mojo_ipc::SourceType::kUnknown;
    }
  }
};

template <>
struct TypeConverter<lorgnette::ColorMode, mojo_ipc::ColorMode> {
  static lorgnette::ColorMode Convert(mojo_ipc::ColorMode mode) {
    switch (mode) {
      case mojo_ipc::ColorMode::kBlackAndWhite:
        return lorgnette::MODE_LINEART;
      case mojo_ipc::ColorMode::kGrayscale:
        return lorgnette::MODE_GRAYSCALE;
      case mojo_ipc::ColorMode::kColor:
        return lorgnette::MODE_COLOR;
    }
  }
};

template <>
struct TypeConverter<lorgnette::ImageFormat, mojo_ipc::FileType> {
  static lorgnette::ImageFormat Convert(mojo_ipc::FileType type) {
    switch (type) {
      case mojo_ipc::FileType::kPng:
        return lorgnette::IMAGE_FORMAT_PNG;
      // PDF images request JPEG data from lorgnette, then
      // convert the returned JPEG data to PDF.
      case mojo_ipc::FileType::kPdf:            // FALLTHROUGH
      case mojo_ipc::FileType::kJpg:
        return lorgnette::IMAGE_FORMAT_JPEG;
    }
  }
};

// static
mojo_ipc::ScanResult
EnumTraits<ash::scanning::mojom::ScanResult, lorgnette::ScanFailureMode>::
    ToMojom(const lorgnette::ScanFailureMode lorgnette_failure_mode) {
  switch (lorgnette_failure_mode) {
    case lorgnette::SCAN_FAILURE_MODE_NO_FAILURE:
      return mojo_ipc::ScanResult::kSuccess;
    case lorgnette::SCAN_FAILURE_MODE_UNKNOWN:
      return mojo_ipc::ScanResult::kUnknownError;
    case lorgnette::SCAN_FAILURE_MODE_DEVICE_BUSY:
      return mojo_ipc::ScanResult::kDeviceBusy;
    case lorgnette::SCAN_FAILURE_MODE_ADF_JAMMED:
      return mojo_ipc::ScanResult::kAdfJammed;
    case lorgnette::SCAN_FAILURE_MODE_ADF_EMPTY:
      return mojo_ipc::ScanResult::kAdfEmpty;
    case lorgnette::SCAN_FAILURE_MODE_FLATBED_OPEN:
      return mojo_ipc::ScanResult::kFlatbedOpen;
    case lorgnette::SCAN_FAILURE_MODE_IO_ERROR:
      return mojo_ipc::ScanResult::kIoError;
    case lorgnette::ScanFailureMode_INT_MIN_SENTINEL_DO_NOT_USE_:
    case lorgnette::ScanFailureMode_INT_MAX_SENTINEL_DO_NOT_USE_:
      break;
  }

  NOTREACHED();
  return mojo_ipc::ScanResult::kUnknownError;
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
    mojo_source->type = mojo::ConvertTo<mojo_ipc::SourceType>(source.type());
    mojo_source->name = source.name();
    mojo_source->page_sizes = GetSupportedPageSizes(source.area());

    mojo_source->color_modes.reserve(source.color_modes().size());
    for (const auto& mode : source.color_modes()) {
      mojo_source->color_modes.push_back(mojo::ConvertTo<mojo_ipc::ColorMode>(
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
  lorgnette_settings.set_source_name(mojo_settings->source_name);
  lorgnette_settings.set_color_mode(
      mojo::ConvertTo<lorgnette::ColorMode>(mojo_settings->color_mode));
  lorgnette_settings.set_resolution(mojo_settings->resolution_dpi);
  lorgnette_settings.set_image_format(
      mojo::ConvertTo<lorgnette::ImageFormat>(mojo_settings->file_type));
  SetScanRegion(mojo_settings->page_size, lorgnette_settings);
  return lorgnette_settings;
}

}  // namespace mojo
