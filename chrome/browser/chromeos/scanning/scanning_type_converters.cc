// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/scanning/scanning_type_converters.h"

#include "base/notreached.h"
#include "chromeos/dbus/lorgnette/lorgnette_service.pb.h"

namespace mojo {

namespace {

namespace mojo_ipc = chromeos::scanning::mojom;

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

// static
mojo_ipc::ScannerCapabilitiesPtr TypeConverter<mojo_ipc::ScannerCapabilitiesPtr,
                                               lorgnette::ScannerCapabilities>::
    Convert(const lorgnette::ScannerCapabilities& lorgnette_caps) {
  mojo_ipc::ScannerCapabilities mojo_caps;
  mojo_caps.sources.reserve(lorgnette_caps.sources().size());
  for (const auto& source : lorgnette_caps.sources()) {
    mojo_caps.sources.push_back(mojo_ipc::ScanSource::New(
        mojo::ConvertTo<mojo_ipc::SourceType>(source.type()), source.name()));
  }

  mojo_caps.color_modes.reserve(lorgnette_caps.color_modes().size());
  for (const auto& mode : lorgnette_caps.color_modes()) {
    mojo_caps.color_modes.push_back(mojo::ConvertTo<mojo_ipc::ColorMode>(
        static_cast<lorgnette::ColorMode>(mode)));
  }

  mojo_caps.resolutions.reserve(lorgnette_caps.resolutions().size());
  for (const auto& res : lorgnette_caps.resolutions())
    mojo_caps.resolutions.push_back(res);

  return mojo_caps.Clone();
}

}  // namespace mojo
