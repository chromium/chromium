// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "base/json/values_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chromeos/crosapi/mojom/print_preview_cros.mojom.h"
#include "components/printing/common/print.mojom.h"
#include "printing/mojom/print.mojom.h"
#include "printing/print_job_constants.h"
#include "printing/print_settings_conversion_chromeos.h"
#include "url/gurl.h"

namespace chromeos {

base::Value::Dict SerializePrintSettings(
    const crosapi::mojom::PrintSettingsPtr& settings) {
  base::Value::Dict dict;
  dict.Set(::printing::kPreviewRequestID,
           static_cast<int>(settings->request_id));
  dict.Set(::printing::kPreviewUIID, static_cast<int>(settings->preview_id));
  dict.Set(::printing::kIsFirstRequest, settings->is_first_request);
  dict.Set(::printing::kSettingPrinterType,
           static_cast<int>(settings->printer_type));
  dict.Set(::printing::kSettingMarginsType,
           static_cast<int>(settings->margin_type));
  dict.Set(::printing::kSettingScalingType,
           static_cast<int>(settings->scaling_type));
  dict.Set(::printing::kSettingCollate, settings->collate);
  dict.Set(::printing::kSettingCopies, static_cast<int>(settings->copies));
  dict.Set(::printing::kSettingColor, static_cast<int>(settings->color));
  dict.Set(::printing::kSettingDuplexMode, static_cast<int>(settings->duplex));
  dict.Set(::printing::kSettingLandscape, settings->landscape);
  dict.Set(::printing::kSettingScaleFactor,
           static_cast<int>(settings->scale_factor));
  dict.Set(::printing::kSettingRasterizePdf, settings->rasterize_pdf);
  dict.Set(::printing::kSettingPagesPerSheet,
           static_cast<int>(settings->pages_per_sheet));
  dict.Set(::printing::kSettingDpiHorizontal,
           static_cast<int>(settings->dpi_horizontal));
  dict.Set(::printing::kSettingDpiVertical,
           static_cast<int>(settings->dpi_vertical));
  dict.Set(::printing::kSettingHeaderFooterEnabled,
           settings->header_footer_enabled);
  dict.Set(::printing::kSettingShouldPrintBackgrounds,
           settings->should_print_backgrounds);
  dict.Set(::printing::kSettingShouldPrintSelectionOnly,
           settings->should_print_selection_only);

  if (settings->title) {
    dict.Set(::printing::kSettingHeaderFooterTitle, *settings->title);
  }
  if (settings->url) {
    dict.Set(::printing::kSettingHeaderFooterURL, (*settings->url).spec());
  }

  if (settings->device_name) {
    dict.Set(::printing::kSettingDeviceName, *settings->device_name);
  }

  if (settings->rasterize_pdf_dpi) {
    dict.Set(::printing::kSettingRasterizePdfDpi,
             static_cast<int>(*settings->rasterize_pdf_dpi));
  }

  if (settings->borderless) {
    dict.Set(::printing::kSettingBorderless, *settings->borderless);
  }

  if (settings->media_type) {
    dict.Set(::printing::kSettingMediaType, *settings->media_type);
  }

  if (settings->preview_modifiable) {
    dict.Set(::printing::kSettingPreviewModifiable,
             *settings->preview_modifiable);
  }

  if (settings->send_user_info) {
    dict.Set(::printing::kSettingSendUserInfo, *settings->send_user_info);
  }

  if (settings->user_name) {
    dict.Set(::printing::kSettingUsername, *settings->user_name);
  }

  if (settings->chromeos_access_oauth_token) {
    dict.Set(::printing::kSettingChromeOSAccessOAuthToken,
             *settings->chromeos_access_oauth_token);
  }

  if (settings->pin_value) {
    dict.Set(::printing::kSettingPinValue, *settings->pin_value);
  }

  if (settings->printer_manually_selected) {
    dict.Set(::printing::kSettingPrinterManuallySelected,
             *settings->printer_manually_selected);
  }

  if (settings->printer_status_reason) {
    dict.Set(::printing::kSettingPrinterStatusReason,
             static_cast<int>(*settings->printer_status_reason));
  }

  if (settings->capabilities) {
    dict.Set(::printing::kSettingCapabilities, *settings->capabilities);
  }

  if (settings->open_pdf_in_preview) {
    dict.Set(::printing::kSettingOpenPDFInPreview,
             *settings->open_pdf_in_preview);
  }

  if (settings->dpi_default) {
    dict.Set(::printing::kSettingDpiDefault, *settings->dpi_default);
  }

  if (settings->page_count) {
    dict.Set(::printing::kSettingPreviewPageCount,
             static_cast<int>(*settings->page_count));
  }

  if (settings->show_system_dialog) {
    dict.Set(::printing::kSettingShowSystemDialog,
             *settings->show_system_dialog);
  }

  if (settings->ipp_client_info) {
    std::vector<::printing::mojom::IppClientInfo> client_info_list;
    client_info_list.reserve((*settings->ipp_client_info).size());
    for (const ::printing::mojom::IppClientInfoPtr& client_info :
         *settings->ipp_client_info) {
      client_info_list.emplace_back(std::move(*client_info));
    }
    dict.Set(::printing::kSettingIppClientInfo,
             ::printing::ConvertClientInfoToJobSetting(client_info_list));
  }

  if (settings->advanced_settings) {
    base::Value::Dict advanced_settings;
    for (auto& setting : *settings->advanced_settings) {
      advanced_settings.Set(setting.first, setting.second.Clone());
    }
    dict.Set(::printing::kSettingAdvancedSettings,
             std::move(advanced_settings));
  }

  if (settings->page_range.size() == 2) {
    base::Value::Dict page_range;
    page_range.Set(::printing::kSettingPageRangeFrom,
                   static_cast<int>(settings->page_range[0]));
    page_range.Set(::printing::kSettingPageRangeTo,
                   static_cast<int>(settings->page_range[1]));
    base::Value::List page_range_list;
    page_range_list.Append(std::move(page_range));
    dict.Set(::printing::kSettingPageRange, std::move(page_range_list));
  }

  base::Value::Dict margins_custom;
  margins_custom.Set(::printing::kSettingMarginTop,
                     static_cast<int>(settings->margins_custom->margin_top));
  margins_custom.Set(::printing::kSettingMarginRight,
                     static_cast<int>(settings->margins_custom->margin_right));
  margins_custom.Set(::printing::kSettingMarginLeft,
                     static_cast<int>(settings->margins_custom->margin_left));
  margins_custom.Set(::printing::kSettingMarginBottom,
                     static_cast<int>(settings->margins_custom->margin_bottom));
  dict.Set(::printing::kSettingMarginsCustom, std::move(margins_custom));

  base::Value::Dict media_size;
  media_size.Set(::printing::kSettingMediaSizeHeightMicrons,
                 static_cast<int>(settings->media_size->height_microns));
  media_size.Set(::printing::kSettingMediaSizeWidthMicrons,
                 static_cast<int>(settings->media_size->width_microns));
  media_size.Set(
      ::printing::kSettingsImageableAreaLeftMicrons,
      static_cast<int>(settings->media_size->imageable_area_left_microns));
  media_size.Set(
      ::printing::kSettingsImageableAreaTopMicrons,
      static_cast<int>(settings->media_size->imageable_area_top_microns));
  media_size.Set(
      ::printing::kSettingsImageableAreaRightMicrons,
      static_cast<int>(settings->media_size->imageable_area_right_microns));
  media_size.Set(
      ::printing::kSettingsImageableAreaBottomMicrons,
      static_cast<int>(settings->media_size->imageable_area_bottom_microns));
  if (settings->media_size->vendor_id) {
    media_size.Set(::printing::kSettingMediaSizeVendorId,
                   *settings->media_size->vendor_id);
  }
  if (settings->media_size->is_default) {
    media_size.Set(::printing::kSettingMediaSizeIsDefault,
                   *settings->media_size->is_default);
  }
  dict.Set(::printing::kSettingMediaSize, std::move(media_size));

  return dict;
}

}  // namespace chromeos
