// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PWG_RASTER_CONVERTER_H_
#define CHROME_BROWSER_PRINTING_PWG_RASTER_CONVERTER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/ref_counted_memory.h"
#include "printing/pdf_render_settings.h"

namespace cloud_devices {
class CloudDeviceDescription;
}

namespace gfx {
class Size;
}

namespace printing {

struct PwgRasterSettings;

class PwgRasterConverter {
 public:
  // Callback for when the PDF is converted to a PWG raster.
  // `region` contains the PWG raster data.
  using ResultCallback =
      base::OnceCallback<void(base::ReadOnlySharedMemoryRegion /*region*/)>;

  virtual ~PwgRasterConverter() {}

  static std::unique_ptr<PwgRasterConverter> CreateDefault();

  // Generates conversion settings to be used with converter from printer
  // capabilities and page size.
  // TODO(vitalybuka): Extract page size from pdf document data.
  static PdfRenderSettings GetConversionSettings(
      const cloud_devices::CloudDeviceDescription& printer_capabilities,
      const gfx::Size& page_size,
      bool use_color);

  // Generates pwg bitmap settings to be used with the converter from
  // device capabilites and printing ticket.
  static PwgRasterSettings GetBitmapSettings(
      const cloud_devices::CloudDeviceDescription& printer_capabilities,
      const cloud_devices::CloudDeviceDescription& ticket);

  virtual void Start(const std::optional<bool>& use_skia,
                     const base::RefCountedMemory* data,
                     const PdfRenderSettings& conversion_settings,
                     const PwgRasterSettings& bitmap_settings,
                     ResultCallback callback) = 0;
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PWG_RASTER_CONVERTER_H_
