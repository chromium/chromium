// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/print_preview/print_settings_converter.h"

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chromeos/crosapi/mojom/print_preview_cros.mojom.h"
#include "components/printing/common/print.mojom.h"
#include "printing/mojom/print.mojom-shared.h"
#include "printing/mojom/print.mojom.h"
#include "printing/print_job_constants.h"
#include "printing/print_settings_conversion_chromeos.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace chromeos {

namespace {

constexpr char kAdvancedSettingKey[] = "foo";
constexpr char kUrl[] = "https://www.google.com";
constexpr char kVendorId[] = "google";
constexpr int kMargin = 1;
constexpr int kAdvancedSettingValue = 1;
constexpr int kMediaSize = 100;

}  // namespace

class PrintSettingsConverterTest : public testing::Test {
 public:
  PrintSettingsConverterTest() = default;

  PrintSettingsConverterTest(const PrintSettingsConverterTest&) = delete;
  PrintSettingsConverterTest& operator=(const PrintSettingsConverterTest&) =
      delete;

  ~PrintSettingsConverterTest() override = default;
};

TEST_F(PrintSettingsConverterTest, SerializePrintSettings) {
  const uint32_t preview_id = 1;
  const uint32_t request_id = 1;
  const bool is_first_request = true;
  const ::printing::mojom::PrinterType printer_type =
      ::printing::mojom::PrinterType::kLocal;
  const ::printing::mojom::MarginType margin_type =
      ::printing::mojom::MarginType::kDefaultMargins;
  const crosapi::mojom::ScalingType scaling_type =
      crosapi::mojom::ScalingType::kCustom;
  const bool collate = true;
  const uint32_t copies = 1;
  const ::printing::mojom::ColorModel color =
      ::printing::mojom::ColorModel::kColor;
  const ::printing::mojom::DuplexMode duplex_mode =
      ::printing::mojom::DuplexMode::kSimplex;
  const bool landscape = true;
  const uint32_t scale_factor = 1;
  const bool rasterize_pdf = true;
  const uint32_t pages_per_sheet = 1;
  const uint32_t dpi_horizontal = 100;
  const uint32_t dpi_vertical = 100;
  const bool header_footer_enabled = true;
  const bool should_print_backgrounds = true;
  const bool should_print_selection_only = true;
  const std::u16string_view title = u"Title";
  const GURL url(kUrl);
  const std::u16string_view device_name = u"device";
  const bool borderless = true;
  const std::u16string_view media_type = u"LETTER";
  const bool preview_modifiable = true;
  const bool send_user_info = false;
  const std::u16string_view user_name = u"username";
  const std::u16string_view chromeos_access_oauth_token = u"123abc";
  const std::u16string_view pin_value = u"pin";
  const bool printer_manually_selected = true;
  const crosapi::mojom::StatusReason::Reason status_reason =
      crosapi::mojom::StatusReason::Reason::kDeviceError;
  const std::u16string_view capabilities = u"{has_selection: true}";
  const bool open_pdf_in_preview = true;
  const bool dpi_default = true;
  const uint32_t page_count = 1;
  const bool show_system_dialog = false;
  const std::vector<uint32_t> page_range({1, 2});

  std::vector<::printing::mojom::IppClientInfoPtr> client_info_list;
  const ::printing::mojom::IppClientInfoPtr client_info =
      ::printing::mojom::IppClientInfo::New();
  client_info_list.push_back(client_info.Clone());

  base::flat_map<std::string, base::Value> advanced_settings;
  advanced_settings.insert_or_assign(kAdvancedSettingKey, base::Value(1));

  crosapi::mojom::MarginsCustomPtr margins_custom =
      crosapi::mojom::MarginsCustom::New();
  margins_custom->margin_top = kMargin;
  margins_custom->margin_bottom = kMargin;
  margins_custom->margin_left = kMargin;
  margins_custom->margin_right = kMargin;

  crosapi::mojom::MediaSizePtr media_size = crosapi::mojom::MediaSize::New();
  media_size->width_microns = kMediaSize;
  media_size->height_microns = kMediaSize;
  media_size->imageable_area_bottom_microns = kMediaSize;
  media_size->imageable_area_top_microns = kMediaSize;
  media_size->imageable_area_left_microns = kMediaSize;
  media_size->imageable_area_right_microns = kMediaSize;
  media_size->vendor_id = base::UTF8ToUTF16(std::string(kVendorId));
  media_size->custom_display_name = u"custom_display_name";
  media_size->is_default = true;
  media_size->name = u"name";

  auto print_settings = crosapi::mojom::PrintSettings::New();
  print_settings->preview_id = preview_id;
  print_settings->request_id = request_id;
  print_settings->is_first_request = is_first_request;
  print_settings->printer_type = printer_type;
  print_settings->margin_type = margin_type;
  print_settings->scaling_type = scaling_type;
  print_settings->margin_type = margin_type;
  print_settings->collate = collate;
  print_settings->copies = copies;
  print_settings->color = color;
  print_settings->duplex = duplex_mode;
  print_settings->landscape = landscape;
  print_settings->scale_factor = scale_factor;
  print_settings->rasterize_pdf = rasterize_pdf;
  print_settings->pages_per_sheet = pages_per_sheet;
  print_settings->dpi_horizontal = dpi_horizontal;
  print_settings->dpi_vertical = dpi_vertical;
  print_settings->header_footer_enabled = header_footer_enabled;
  print_settings->should_print_backgrounds = should_print_backgrounds;
  print_settings->should_print_selection_only = should_print_selection_only;
  print_settings->title = title;
  print_settings->url = url;
  print_settings->device_name = device_name;
  print_settings->borderless = borderless;
  print_settings->media_type = media_type;
  print_settings->preview_modifiable = preview_modifiable;
  print_settings->send_user_info = send_user_info;
  print_settings->user_name = user_name;
  print_settings->chromeos_access_oauth_token = chromeos_access_oauth_token;
  print_settings->pin_value = pin_value;
  print_settings->printer_manually_selected = printer_manually_selected;
  print_settings->printer_status_reason = status_reason;
  print_settings->capabilities = capabilities;
  print_settings->open_pdf_in_preview = open_pdf_in_preview;
  print_settings->dpi_default = dpi_default;
  print_settings->page_count = page_count;
  print_settings->show_system_dialog = show_system_dialog;
  print_settings->ipp_client_info = mojo::Clone(client_info_list);
  print_settings->advanced_settings = mojo::Clone(advanced_settings);
  print_settings->page_range = page_range;
  print_settings->margins_custom = margins_custom.Clone();
  print_settings->media_size = media_size.Clone();

  // Serialize the settings.
  base::Value::Dict serialized_settings =
      SerializePrintSettings(print_settings);

  // Validate the serialization.
  EXPECT_EQ(static_cast<int>(preview_id),
            *serialized_settings.FindInt(::printing::kPreviewUIID));
  EXPECT_EQ(static_cast<int>(request_id),
            *serialized_settings.FindInt(::printing::kPreviewRequestID));
  EXPECT_EQ(is_first_request,
            *serialized_settings.FindBool(::printing::kIsFirstRequest));
  EXPECT_EQ(static_cast<int>(printer_type),
            *serialized_settings.FindInt(::printing::kSettingPrinterType));
  EXPECT_EQ(static_cast<int>(margin_type),
            *serialized_settings.FindInt(::printing::kSettingMarginsType));
  EXPECT_EQ(static_cast<int>(scaling_type),
            *serialized_settings.FindInt(::printing::kSettingScalingType));
  EXPECT_EQ(collate,
            *serialized_settings.FindBool(::printing::kSettingCollate));
  EXPECT_EQ(static_cast<int>(copies),
            *serialized_settings.FindInt(::printing::kSettingCopies));
  EXPECT_EQ(static_cast<int>(color),
            *serialized_settings.FindInt(::printing::kSettingColor));
  EXPECT_EQ(static_cast<int>(duplex_mode),
            *serialized_settings.FindInt(::printing::kSettingDuplexMode));
  EXPECT_EQ(landscape,
            *serialized_settings.FindBool(::printing::kSettingLandscape));
  EXPECT_EQ(static_cast<int>(scale_factor),
            *serialized_settings.FindInt(::printing::kSettingScaleFactor));
  EXPECT_EQ(rasterize_pdf,
            *serialized_settings.FindBool(::printing::kSettingRasterizePdf));
  EXPECT_EQ(static_cast<int>(pages_per_sheet),
            *serialized_settings.FindInt(::printing::kSettingPagesPerSheet));
  EXPECT_EQ(static_cast<int>(dpi_horizontal),
            *serialized_settings.FindInt(::printing::kSettingDpiHorizontal));
  EXPECT_EQ(static_cast<int>(dpi_vertical),
            *serialized_settings.FindInt(::printing::kSettingDpiVertical));
  EXPECT_EQ(
      header_footer_enabled,
      *serialized_settings.FindBool(::printing::kSettingHeaderFooterEnabled));
  EXPECT_EQ(should_print_backgrounds,
            *serialized_settings.FindBool(
                ::printing::kSettingShouldPrintBackgrounds));
  EXPECT_EQ(should_print_selection_only,
            *serialized_settings.FindBool(
                ::printing::kSettingShouldPrintSelectionOnly));
  std::string expected_title;
  base::UTF16ToUTF8(title.data(), title.size(), &expected_title);
  EXPECT_EQ(expected_title, *serialized_settings.FindString(
                                ::printing::kSettingHeaderFooterTitle));
  EXPECT_EQ(url.spec(), *serialized_settings.FindString(
                            ::printing::kSettingHeaderFooterURL));
  std::string expected_device_name;
  base::UTF16ToUTF8(device_name.data(), device_name.size(),
                    &expected_device_name);
  EXPECT_EQ(expected_device_name,
            *serialized_settings.FindString(::printing::kSettingDeviceName));
  EXPECT_EQ(borderless,
            *serialized_settings.FindBool(::printing::kSettingBorderless));
  std::string expected_media_type;
  base::UTF16ToUTF8(media_type.data(), media_type.size(), &expected_media_type);
  EXPECT_EQ(expected_media_type,
            *serialized_settings.FindString(::printing::kSettingMediaType));
  EXPECT_EQ(preview_modifiable, *serialized_settings.FindBool(
                                    ::printing::kSettingPreviewModifiable));
  EXPECT_EQ(send_user_info,
            *serialized_settings.FindBool(::printing::kSettingSendUserInfo));
  std::string expected_user_name;
  base::UTF16ToUTF8(user_name.data(), user_name.size(), &expected_user_name);
  EXPECT_EQ(expected_user_name,
            *serialized_settings.FindString(::printing::kSettingUsername));
  std::string expected_oath_token;
  base::UTF16ToUTF8(chromeos_access_oauth_token.data(),
                    chromeos_access_oauth_token.size(), &expected_oath_token);
  EXPECT_EQ(expected_oath_token,
            *serialized_settings.FindString(
                ::printing::kSettingChromeOSAccessOAuthToken));
  std::string expected_pin_value;
  base::UTF16ToUTF8(pin_value.data(), pin_value.size(), &expected_pin_value);
  EXPECT_EQ(expected_pin_value,
            *serialized_settings.FindString(::printing::kSettingPinValue));
  EXPECT_EQ(printer_manually_selected,
            *serialized_settings.FindBool(
                ::printing::kSettingPrinterManuallySelected));
  EXPECT_EQ(
      static_cast<int>(status_reason),
      *serialized_settings.FindInt(::printing::kSettingPrinterStatusReason));
  std::string expected_capabilities;
  base::UTF16ToUTF8(capabilities.data(), capabilities.size(),
                    &expected_capabilities);
  EXPECT_EQ(expected_capabilities,
            *serialized_settings.FindString(::printing::kSettingCapabilities));
  EXPECT_EQ(open_pdf_in_preview, *serialized_settings.FindBool(
                                     ::printing::kSettingOpenPDFInPreview));
  EXPECT_EQ(dpi_default,
            *serialized_settings.FindBool(::printing::kSettingDpiDefault));
  EXPECT_EQ(static_cast<int>(page_count),
            *serialized_settings.FindInt(::printing::kSettingPreviewPageCount));
  EXPECT_EQ(show_system_dialog, *serialized_settings.FindBool(
                                    ::printing::kSettingShowSystemDialog));

  const base::Value::List actual_ipp_clients =
      (*serialized_settings.FindList(::printing::kSettingIppClientInfo))
          .Clone();

  EXPECT_EQ(client_info_list.size(), actual_ipp_clients.size());
  // Only one entry in the list.
  EXPECT_EQ(client_info_list[0]->client_name,
            *(actual_ipp_clients[0].GetDict().FindString(
                ::printing::kSettingIppClientName)));
  EXPECT_EQ(client_info_list[0]->client_string_version,
            *actual_ipp_clients[0].GetDict().FindString(
                ::printing::kSettingIppClientStringVersion));
  EXPECT_EQ(static_cast<int>(client_info_list[0]->client_type),
            actual_ipp_clients[0].GetDict().FindInt(
                ::printing::kSettingIppClientType));

  const base::Value::List actual_page_range =
      (*serialized_settings.FindList(::printing::kSettingPageRange)).Clone();
  // Only two entries in the list.
  EXPECT_EQ(static_cast<int>(page_range[0]),
            actual_page_range[0].GetDict().FindInt(
                ::printing::kSettingPageRangeFrom));
  EXPECT_EQ(
      static_cast<int>(page_range[1]),
      actual_page_range[0].GetDict().FindInt(::printing::kSettingPageRangeTo));

  const base::Value::Dict actual_margins_custom =
      (*serialized_settings.FindDict(::printing::kSettingMarginsCustom))
          .Clone();
  EXPECT_EQ(kMargin,
            actual_margins_custom.FindInt(::printing::kSettingMarginTop));
  EXPECT_EQ(kMargin,
            actual_margins_custom.FindInt(::printing::kSettingMarginBottom));
  EXPECT_EQ(kMargin,
            actual_margins_custom.FindInt(::printing::kSettingMarginTop));
  EXPECT_EQ(kMargin,
            actual_margins_custom.FindInt(::printing::kSettingMarginTop));

  const base::Value::Dict actual_media_size =
      (*serialized_settings.FindDict(::printing::kSettingMediaSize)).Clone();
  EXPECT_EQ(kMediaSize, actual_media_size.FindInt(
                            ::printing::kSettingMediaSizeWidthMicrons));
  EXPECT_EQ(kMediaSize, actual_media_size.FindInt(
                            ::printing::kSettingMediaSizeHeightMicrons));
  EXPECT_EQ(kMediaSize, actual_media_size.FindInt(
                            printing::kSettingsImageableAreaBottomMicrons));
  EXPECT_EQ(kMediaSize, actual_media_size.FindInt(
                            printing::kSettingsImageableAreaTopMicrons));
  EXPECT_EQ(kMediaSize, actual_media_size.FindInt(
                            printing::kSettingsImageableAreaLeftMicrons));
  EXPECT_EQ(kMediaSize, actual_media_size.FindInt(
                            printing::kSettingsImageableAreaRightMicrons));
  EXPECT_EQ(kVendorId, *actual_media_size.FindString(
                           ::printing::kSettingMediaSizeVendorId));
  EXPECT_EQ(media_size->is_default,
            actual_media_size.FindBool(::printing::kSettingMediaSizeIsDefault));

  const base::Value::Dict actual_advanced_settings =
      (*serialized_settings.FindDict(::printing::kSettingAdvancedSettings))
          .Clone();
  EXPECT_EQ(kAdvancedSettingValue,
            actual_advanced_settings.FindInt(kAdvancedSettingKey));
}

}  // namespace chromeos
