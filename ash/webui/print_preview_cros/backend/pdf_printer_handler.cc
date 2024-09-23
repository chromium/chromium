// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/print_preview_cros/backend/pdf_printer_handler.h"

#include <string>
#include <utility>

#include "ash/webui/print_preview_cros/mojom/printer_capabilities.mojom.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "components/cloud_devices/common/printer_description.h"
#include "printing/mojom/print.mojom.h"
#include "printing/printing_context.h"
#include "printing/units.h"
#include "ui/gfx/geometry/size.h"

namespace ash::printing::print_preview {

namespace {

// PrintingContextDelegate is required to get the localized default media size
// for PDF capabilities.
class PrintingContextDelegate : public ::printing::PrintingContext::Delegate {
 public:
  // PrintingContext::Delegate methods.
  gfx::NativeView GetParentView() override { NOTREACHED(); }
  std::string GetAppLocale() override {
    // TODO(b/323421684): Fetch application locale.
    return "en-US";
  }
};

gfx::Size GetDefaultPdfMediaSize() {
  PrintingContextDelegate delegate;
  // The `PrintingContext` for "Save as PDF" does not need to make system
  // printing calls, it just relies on localization plus hardcoded defaults
  // from `PrintingContext::UsePdfSettings()`. This means that OOP support
  // is unnecessary in this case.
  auto printing_context(::printing::PrintingContext::Create(
      &delegate, ::printing::PrintingContext::ProcessBehavior::kOopDisabled));
  printing_context->UsePdfSettings();
  gfx::Size pdf_media_size = printing_context->GetPdfPaperSizeDeviceUnits();
  float device_microns_per_device_unit =
      static_cast<float>(::printing::kMicronsPerInch) /
      printing_context->settings().device_units_per_inch();
  return gfx::Size(pdf_media_size.width() * device_microns_per_device_unit,
                   pdf_media_size.height() * device_microns_per_device_unit);
}

mojom::PageOrientationCapabilityPtr ConstructPageOrientationCapability() {
  mojom::PageOrientationOptionPtr portrait =
      mojom::PageOrientationOption::New();
  portrait->option = mojom::PageOrientation::kPortrait;
  mojom::PageOrientationOptionPtr landscape =
      mojom::PageOrientationOption::New();
  landscape->option = mojom::PageOrientation::kLandscape;
  mojom::PageOrientationOptionPtr autoOrient =
      mojom::PageOrientationOption::New();
  autoOrient->option = mojom::PageOrientation::kAuto;
  autoOrient->is_default = true;
  mojom::PageOrientationCapabilityPtr page_orientation =
      mojom::PageOrientationCapability::New();
  page_orientation->options.push_back(std::move(portrait));
  page_orientation->options.push_back(std::move(landscape));
  page_orientation->options.push_back(std::move(autoOrient));
  return page_orientation;
}

mojom::ColorCapabilityPtr ConstructColorCapability() {
  mojom::ColorOptionPtr color_option = mojom::ColorOption::New();
  color_option->vendor_id = base::NumberToString(
      static_cast<int>(::printing::mojom::ColorModel::kColor));
  color_option->is_default = true;
  mojom::ColorCapabilityPtr color = mojom::ColorCapability::New();
  color->options.push_back(std::move(color_option));
  return color;
}

mojom::MediaSizeCapabilityPtr ConstructMediaSizeCapability() {
  const gfx::Size default_media_size = GetDefaultPdfMediaSize();
  mojom::MediaSizeOptionPtr default_media_size_option =
      mojom::MediaSizeOption::New();
  // TODO(b/323421684): Use the localized media name.
  default_media_size_option->name = "Letter";
  default_media_size_option->height_microns = default_media_size.height();
  default_media_size_option->width_microns = default_media_size.width();
  default_media_size_option->imageable_area_left_microns = 0;
  default_media_size_option->imageable_area_bottom_microns = 0;
  default_media_size_option->imageable_area_right_microns =
      default_media_size.width();
  default_media_size_option->imageable_area_top_microns =
      default_media_size.height();
  default_media_size_option->is_default = true;
  mojom::MediaSizeCapabilityPtr media_size = mojom::MediaSizeCapability::New();
  media_size->options.push_back(std::move(default_media_size_option));
  return media_size;
}

mojom::DpiCapabilityPtr ConstructDpiCapability() {
  mojom::DpiOptionPtr dpi_option = mojom::DpiOption::New();
  dpi_option->horizontal_dpi = ::printing::kDefaultPdfDpi;
  dpi_option->vertical_dpi = ::printing::kDefaultPdfDpi;
  dpi_option->is_default = true;
  mojom::DpiCapabilityPtr dpi = mojom::DpiCapability::New();
  dpi->options.push_back(std::move(dpi_option));
  return dpi;
}

}  // namespace

mojom::CapabilitiesPtr PdfPrinterHandler::FetchCapabilities(
    const std::string& destination_id) {
  mojom::PageOrientationCapabilityPtr page_orientation =
      ConstructPageOrientationCapability();
  mojom::ColorCapabilityPtr color = ConstructColorCapability();
  mojom::MediaSizeCapabilityPtr media_size = ConstructMediaSizeCapability();
  mojom::DpiCapabilityPtr dpi = ConstructDpiCapability();

  mojom::CapabilitiesPtr caps = mojom::Capabilities::New();
  caps->destination_id = destination_id;
  caps->page_orientation = std::move(page_orientation);
  caps->color = std::move(color);
  caps->media_size = std::move(media_size);
  caps->dpi = std::move(dpi);

  return caps;
}

}  // namespace ash::printing::print_preview
