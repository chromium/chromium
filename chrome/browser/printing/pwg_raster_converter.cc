// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/pwg_raster_converter.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>

#include "base/cancelable_callback.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "chrome/browser/printing/printing_service.h"
#include "chrome/services/printing/public/mojom/pdf_to_pwg_raster_converter.mojom.h"
#include "chrome/services/printing/public/mojom/printing_service.mojom.h"
#include "components/cloud_devices/common/cloud_device_description.h"
#include "components/cloud_devices/common/printer_description.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "printing/mojom/print.mojom.h"
#include "printing/pdf_render_settings.h"
#include "printing/pwg_raster_settings.h"
#include "printing/units.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace printing {

namespace {

using content::BrowserThread;

// Converts PDF into PWG raster. Class lives on the UI thread.
class PwgRasterConverterHelper
    : public base::RefCounted<PwgRasterConverterHelper> {
 public:
  PwgRasterConverterHelper(const PdfRenderSettings& settings,
                           const PwgRasterSettings& bitmap_settings);

  PwgRasterConverterHelper(const PwgRasterConverterHelper&) = delete;
  PwgRasterConverterHelper& operator=(const PwgRasterConverterHelper&) = delete;

  void Convert(const std::optional<bool>& use_skia,
               const base::RefCountedMemory* data,
               PwgRasterConverter::ResultCallback callback);

 private:
  friend class base::RefCounted<PwgRasterConverterHelper>;

  ~PwgRasterConverterHelper();

  void RunCallback(base::ReadOnlySharedMemoryRegion region,
                   uint32_t page_count);

  PdfRenderSettings settings_;
  PwgRasterSettings bitmap_settings_;
  mojo::Remote<printing::mojom::PdfToPwgRasterConverter>
      pdf_to_pwg_raster_converter_remote_;
  PwgRasterConverter::ResultCallback callback_;
};

PwgRasterConverterHelper::PwgRasterConverterHelper(
    const PdfRenderSettings& settings,
    const PwgRasterSettings& bitmap_settings)
    : settings_(settings), bitmap_settings_(bitmap_settings) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

PwgRasterConverterHelper::~PwgRasterConverterHelper() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void PwgRasterConverterHelper::Convert(
    const std::optional<bool>& use_skia,
    const base::RefCountedMemory* data,
    PwgRasterConverter::ResultCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  callback_ = std::move(callback);

  GetPrintingService()->BindPdfToPwgRasterConverter(
      pdf_to_pwg_raster_converter_remote_.BindNewPipeAndPassReceiver());
  pdf_to_pwg_raster_converter_remote_.set_disconnect_handler(
      base::BindOnce(&PwgRasterConverterHelper::RunCallback, this,
                     base::ReadOnlySharedMemoryRegion(), /*page_count=*/0));

  base::MappedReadOnlyRegion memory =
      base::ReadOnlySharedMemoryRegion::Create(data->size());
  if (!memory.IsValid()) {
    RunCallback(base::ReadOnlySharedMemoryRegion(), /*page_count=*/0);
    return;
  }

  if (use_skia) {
    pdf_to_pwg_raster_converter_remote_->SetUseSkiaRendererPolicy(*use_skia);
  }

  // TODO(thestig): Write `data` into shared memory in the first place, to avoid
  // this memcpy().
  memcpy(memory.mapping.memory(), data->data(), data->size());
  pdf_to_pwg_raster_converter_remote_->Convert(
      std::move(memory.region), settings_, bitmap_settings_,
      base::BindOnce(&PwgRasterConverterHelper::RunCallback, this));
}

void PwgRasterConverterHelper::RunCallback(
    base::ReadOnlySharedMemoryRegion region,
    uint32_t page_count) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (callback_) {
    if (region.IsValid() && page_count > 0) {
      size_t average_page_size_in_kb = region.GetSize() / 1024;
      average_page_size_in_kb /= page_count;
      UMA_HISTOGRAM_MEMORY_KB("Printing.ConversionSize.Pwg",
                              average_page_size_in_kb);
      std::move(callback_).Run(std::move(region));
    } else {
      // TODO(thestig): Consider adding UMA to track failure rates.
      std::move(callback_).Run(base::ReadOnlySharedMemoryRegion());
    }
  }
  pdf_to_pwg_raster_converter_remote_.reset();
}

class PwgRasterConverterImpl : public PwgRasterConverter {
 public:
  PwgRasterConverterImpl();

  PwgRasterConverterImpl(const PwgRasterConverterImpl&) = delete;
  PwgRasterConverterImpl& operator=(const PwgRasterConverterImpl&) = delete;

  ~PwgRasterConverterImpl() override;

  void Start(const std::optional<bool>& use_skia,
             const base::RefCountedMemory* data,
             const PdfRenderSettings& conversion_settings,
             const PwgRasterSettings& bitmap_settings,
             ResultCallback callback) override;

 private:
  scoped_refptr<PwgRasterConverterHelper> utility_client_;

  // Cancelable version of PwgRasterConverter::ResultCallback.
  base::CancelableOnceCallback<void(base::ReadOnlySharedMemoryRegion)>
      cancelable_callback_;
};

PwgRasterConverterImpl::PwgRasterConverterImpl() = default;

PwgRasterConverterImpl::~PwgRasterConverterImpl() = default;

void PwgRasterConverterImpl::Start(const std::optional<bool>& use_skia,
                                   const base::RefCountedMemory* data,
                                   const PdfRenderSettings& conversion_settings,
                                   const PwgRasterSettings& bitmap_settings,
                                   ResultCallback callback) {
  cancelable_callback_.Reset(std::move(callback));
  utility_client_ = base::MakeRefCounted<PwgRasterConverterHelper>(
      conversion_settings, bitmap_settings);
  utility_client_->Convert(use_skia, data, cancelable_callback_.callback());
}

}  // namespace

// static
std::unique_ptr<PwgRasterConverter> PwgRasterConverter::CreateDefault() {
  return std::make_unique<PwgRasterConverterImpl>();
}

// static
PdfRenderSettings PwgRasterConverter::GetConversionSettings(
    const cloud_devices::CloudDeviceDescription& printer_capabilities,
    const gfx::Size& page_size,
    bool use_color) {
  gfx::Size dpi = gfx::Size(kDefaultPdfDpi, kDefaultPdfDpi);
  cloud_devices::printer::DpiCapability dpis;
  if (dpis.LoadFrom(printer_capabilities))
    dpi = gfx::Size(dpis.GetDefault().horizontal, dpis.GetDefault().vertical);

  bool page_is_landscape =
      static_cast<double>(page_size.width()) / dpi.width() >
      static_cast<double>(page_size.height()) / dpi.height();

  // Pdfium assumes that page width is given in dpi.width(), and height in
  // dpi.height(). If we rotate the page, we need to also swap the DPIs.
  gfx::Size final_page_size = page_size;
  if (page_is_landscape) {
    final_page_size = gfx::Size(page_size.height(), page_size.width());
    dpi = gfx::Size(dpi.height(), dpi.width());
  }
  double scale_x = static_cast<double>(dpi.width()) / kPointsPerInch;
  double scale_y = static_cast<double>(dpi.height()) / kPointsPerInch;

  // Make vertical rectangle to optimize streaming to printer. Fix orientation
  // by autorotate.
  gfx::Rect area(final_page_size.width() * scale_x,
                 final_page_size.height() * scale_y);
  return PdfRenderSettings(area, gfx::Point(0, 0), dpi,
                           /*autorotate=*/true, use_color,
                           PdfRenderSettings::Mode::NORMAL);
}

// static
PwgRasterSettings PwgRasterConverter::GetBitmapSettings(
    const cloud_devices::CloudDeviceDescription& printer_capabilities,
    const cloud_devices::CloudDeviceDescription& ticket) {
  cloud_devices::printer::DuplexTicketItem duplex_item;
  cloud_devices::printer::DuplexType duplex_value =
      cloud_devices::printer::DuplexType::NO_DUPLEX;
  if (duplex_item.LoadFrom(ticket))
    duplex_value = duplex_item.value();

  // This assumes `ticket` contains a color ticket item. In case it does not, or
  // the color is invalid, `color_value` will default to AUTO_COLOR, which works
  // just fine. With AUTO_COLOR, it may be possible to better determine the
  // value for `use_color` based on `printer_capabilities`, rather than just
  // defaulting to the safe value of true. Parsing `printer_capabilities`
  // requires work, which this method is avoiding on purpose.
  cloud_devices::printer::Color color_value;
  cloud_devices::printer::ColorTicketItem color_item;
  if (color_item.LoadFrom(ticket) && color_item.IsValid())
    color_value = color_item.value();
  DCHECK(color_value.IsValid());
  bool use_color;
  switch (color_value.type) {
    case cloud_devices::printer::ColorType::STANDARD_MONOCHROME:
    case cloud_devices::printer::ColorType::CUSTOM_MONOCHROME:
      use_color = false;
      break;

    case cloud_devices::printer::ColorType::STANDARD_COLOR:
    case cloud_devices::printer::ColorType::CUSTOM_COLOR:
    case cloud_devices::printer::ColorType::AUTO_COLOR:
      use_color = true;
      break;

    default:
      NOTREACHED_IN_MIGRATION();
      use_color = true;  // Still need to initialize `color` or MSVC will warn.
      break;
  }

  cloud_devices::printer::PwgRasterConfigCapability raster_capability;
  // If the raster capability fails to load, `raster_capability` will contain
  // the default value.
  raster_capability.LoadFrom(printer_capabilities);
  cloud_devices::printer::DocumentSheetBack document_sheet_back =
      raster_capability.value().document_sheet_back;

  PwgRasterSettings result;
  switch (duplex_value) {
    case cloud_devices::printer::DuplexType::NO_DUPLEX:
      result.duplex_mode = mojom::DuplexMode::kSimplex;
      result.odd_page_transform = TRANSFORM_NORMAL;
      break;
    case cloud_devices::printer::DuplexType::LONG_EDGE:
      result.duplex_mode = mojom::DuplexMode::kLongEdge;
      if (document_sheet_back ==
          cloud_devices::printer::DocumentSheetBack::ROTATED) {
        result.odd_page_transform = TRANSFORM_ROTATE_180;
      } else if (document_sheet_back ==
                 cloud_devices::printer::DocumentSheetBack::FLIPPED) {
        result.odd_page_transform = TRANSFORM_FLIP_VERTICAL;
      } else {
        result.odd_page_transform = TRANSFORM_NORMAL;
      }
      break;
    case cloud_devices::printer::DuplexType::SHORT_EDGE:
      result.duplex_mode = mojom::DuplexMode::kShortEdge;
      if (document_sheet_back ==
          cloud_devices::printer::DocumentSheetBack::MANUAL_TUMBLE) {
        result.odd_page_transform = TRANSFORM_ROTATE_180;
      } else if (document_sheet_back ==
                 cloud_devices::printer::DocumentSheetBack::FLIPPED) {
        result.odd_page_transform = TRANSFORM_FLIP_HORIZONTAL;
      } else {
        result.odd_page_transform = TRANSFORM_NORMAL;
      }
      break;
  }

  result.rotate_all_pages = raster_capability.value().rotate_all_pages;
  result.reverse_page_order = raster_capability.value().reverse_order_streaming;

  // No need to check for SRGB_8 support in `types`. CDD spec says:
  // "any printer that doesn't support SGRAY_8 must be able to perform
  // conversion from RGB to grayscale... "
  const auto& types = raster_capability.value().document_types_supported;
  result.use_color =
      use_color ||
      !base::Contains(
          types, cloud_devices::printer::PwgDocumentTypeSupported::SGRAY_8);

  return result;
}

}  // namespace printing
