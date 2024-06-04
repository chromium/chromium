// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/print_preview_cros/backend/destination_provider.h"

#include <string>
#include <utility>

#include "ash/webui/print_preview_cros/mojom/destination_provider.mojom.h"
#include "ash/webui/print_preview_cros/mojom/printer_capabilities.mojom.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "components/cloud_devices/common/printer_description.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "printing/mojom/print.mojom.h"
#include "printing/printing_context.h"
#include "printing/units.h"
#include "ui/gfx/geometry/size.h"

namespace ash::printing::print_preview {

namespace {

// PrintingContextDelegate required to get the localized default media size for
// PDF capabilities.
class PrintingContextDelegate : public ::printing::PrintingContext::Delegate {
 public:
  // PrintingContext::Delegate methods.
  gfx::NativeView GetParentView() override { NOTREACHED_NORETURN(); }
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
  const int units_per_inch =
      printing_context->settings().device_units_per_inch();
  CHECK(units_per_inch > 0);
  float device_microns_per_device_unit =
      static_cast<float>(::printing::kMicronsPerInch) / units_per_inch;
  return gfx::Size(pdf_media_size.width() * device_microns_per_device_unit,
                   pdf_media_size.height() * device_microns_per_device_unit);
}

}  // namespace

DestinationProvider::DestinationProvider() {}
DestinationProvider::~DestinationProvider() = default;

void DestinationProvider::BindInterface(
    mojo::PendingReceiver<mojom::DestinationProvider> pending_receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(pending_receiver));
}

void DestinationProvider::FetchCapabilities(
    const std::string& destination_id,
    ::printing::mojom::PrinterType printerType,
    FetchCapabilitiesCallback callback) {
  if (printerType != ::printing::mojom::PrinterType::kPdf) {
    mojo::ReportBadMessage("No support for non-PDF destinations");
    return;
  }

  // TODO(b/323421684): Move below logic to PDF specific handler.
  // Page Orientation capability.
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

  // Color capability.
  mojom::ColorOptionPtr color_option = mojom::ColorOption::New();
  color_option->type = mojom::ColorType::kStandardColor;
  color_option->vendor_id = base::NumberToString(
      static_cast<int>(::printing::mojom::ColorModel::kColor));
  color_option->is_default = true;
  mojom::ColorCapabilityPtr color = mojom::ColorCapability::New();
  color->options.push_back(std::move(color_option));

  // Media Size capability.
  // TODO(b/323421684): Include all default media sizes.
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

  // DPI capability.
  mojom::DpiOptionPtr dpi_option = mojom::DpiOption::New();
  dpi_option->horizontal_dpi = ::printing::kDefaultPdfDpi;
  dpi_option->vertical_dpi = ::printing::kDefaultPdfDpi;
  dpi_option->is_default = true;
  mojom::DpiCapabilityPtr dpi = mojom::DpiCapability::New();
  dpi->options.push_back(std::move(dpi_option));

  mojom::CapabilitiesPtr caps = mojom::Capabilities::New();
  caps->destination_id = destination_id;
  caps->page_orientation = std::move(page_orientation);
  caps->color = std::move(color);
  caps->media_size = std::move(media_size);
  caps->dpi = std::move(dpi);

  std::move(callback).Run(std::move(caps));
}

}  // namespace ash::printing::print_preview
