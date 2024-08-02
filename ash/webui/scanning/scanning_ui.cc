// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/webui/scanning/scanning_ui.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/webui/common/backend/accessibility_features.h"
#include "ash/webui/common/mojom/accessibility_features.mojom.h"
#include "ash/webui/common/trusted_types_util.h"
#include "ash/webui/grit/ash_scanning_app_resources.h"
#include "ash/webui/grit/ash_scanning_app_resources_map.h"
#include "ash/webui/scanning/mojom/scanning.mojom.h"
#include "ash/webui/scanning/scanning_app_delegate.h"
#include "ash/webui/scanning/scanning_metrics_handler.h"
#include "ash/webui/scanning/url_constants.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/resources/grit/webui_resources.h"
#include "ui/webui/color_change_listener/color_change_handler.h"

namespace ash {

namespace {

void SetUpWebUIDataSource(content::WebUIDataSource* source,
                          base::span<const webui::ResourcePath> resources,
                          int default_resource) {
  for (const auto& resource : resources) {
    source->AddResourcePath(resource.path, resource.id);
  }

  source->SetDefaultResource(default_resource);
  source->AddResourcePath("test_loader.html", IDR_WEBUI_TEST_LOADER_HTML);
  source->AddResourcePath("test_loader.js", IDR_WEBUI_JS_TEST_LOADER_JS);
  source->AddResourcePath("test_loader_util.js",
                          IDR_WEBUI_JS_TEST_LOADER_UTIL_JS);
}

void AddScanningAppStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"a3OptionText", IDS_SCANNING_APP_A3_OPTION_TEXT},
      {"a4OptionText", IDS_SCANNING_APP_A4_OPTION_TEXT},
      {"actionToolbarPageCountText",
       IDS_SCANNING_APP_ACTION_TOOLBAR_PAGE_COUNT_TEXT},
      {"appTitle", IDS_SCANNING_APP_TITLE},
      {"b4OptionText", IDS_SCANNING_APP_B4_OPTION_TEXT},
      {"blackAndWhiteOptionText", IDS_SCANNING_APP_BLACK_AND_WHITE_OPTION_TEXT},
      {"cancelButtonText", IDS_SCANNING_APP_CANCEL_BUTTON_TEXT},
      {"cancelFailedToastText", IDS_SCANNING_APP_CANCEL_FAILED_TOAST_TEXT},
      {"cancelingScanningText", IDS_SCANNING_APP_CANCELING_SCANNING_TEXT},
      {"colorModeDropdownLabel", IDS_SCANNING_APP_COLOR_MODE_DROPDOWN_LABEL},
      {"colorOptionText", IDS_SCANNING_APP_COLOR_OPTION_TEXT},
      {"defaultSourceOptionText", IDS_SCANNING_APP_DEFAULT_SOURCE_OPTION_TEXT},
      {"doneButtonText", IDS_SCANNING_APP_DONE_BUTTON_TEXT},
      {"editButtonLabel", IDS_SCANNING_APP_EDIT_BUTTON_LABEL},
      {"fileNotFoundToastText", IDS_SCANNING_APP_FILE_NOT_FOUND_TOAST_TEXT},
      {"fileTypeDropdownLabel", IDS_SCANNING_APP_FILE_TYPE_DROPDOWN_LABEL},
      {"fitToScanAreaOptionText",
       IDS_SCANNING_APP_FIT_TO_SCAN_AREA_OPTION_TEXT},
      {"flatbedOptionText", IDS_SCANNING_APP_FLATBED_OPTION_TEXT},
      {"getHelpLinkText", IDS_SCANNING_APP_GET_HELP_LINK_TEXT},
      {"grayscaleOptionText", IDS_SCANNING_APP_GRAYSCALE_OPTION_TEXT},
      {"jpgOptionText", IDS_SCANNING_APP_JPG_OPTION_TEXT},
      {"learnMoreButtonLabel", IDS_SCANNING_APP_LEARN_MORE_BUTTON_LABEL},
      {"legalOptionText", IDS_SCANNING_APP_LEGAL_OPTION_TEXT},
      {"letterOptionText", IDS_SCANNING_APP_LETTER_OPTION_TEXT},
      {"moreSettings", IDS_SCANNING_APP_MORE_SETTINGS},
      {"multiPageCancelingScanningText",
       IDS_SCANNING_APP_MULTI_PAGE_CANCELING_SCANNING_TEXT},
      {"multiPageCheckboxAriaLabel",
       IDS_SCANNING_APP_MULTI_PAGE_CHECKBOX_ARIA_LABEL},
      {"multiPageCheckboxText", IDS_SCANNING_APP_MULTI_PAGE_CHECKBOX_TEXT},
      {"multiPageImageAriaLabel", IDS_SCANNING_APP_MULTI_PAGE_IMAGE_ARIA_LABEL},
      {"multiPageScanInstructionsText",
       IDS_SCANNING_APP_MULTI_PAGE_SCAN_INSTRUCTIONS_TEXT},
      {"multiPageScanProgressText",
       IDS_SCANNING_APP_MULTI_PAGE_SCAN_PROGRESS_TEXT},
      {"myFilesSelectOption", IDS_SCANNING_APP_MY_FILES_SELECT_OPTION},
      {"noScannersText", IDS_SCANNING_APP_NO_SCANNERS_TEXT},
      {"noScannersSubtext", IDS_SCANNING_APP_NO_SCANNERS_SUBTEXT},
      {"okButtonLabel", IDS_SCANNING_APP_OK_BUTTON_LABEL},
      {"oneSidedDocFeederOptionText",
       IDS_SCANNING_APP_ONE_SIDED_DOC_FEEDER_OPTION_TEXT},
      {"pdfOptionText", IDS_SCANNING_APP_PDF_OPTION_TEXT},
      {"pngOptionText", IDS_SCANNING_APP_PNG_OPTION_TEXT},
      {"pageSizeDropdownLabel", IDS_SCANNING_APP_PAGE_SIZE_DROPDOWN_LABEL},
      {"removePageButtonLabel", IDS_SCANNING_APP_REMOVE_PAGE_BUTTON_LABEL},
      {"removePageConfirmationText",
       IDS_SCANNING_APP_REMOVE_PAGE_CONFIRMATION_TEXT},
      {"rescanPageButtonLabel", IDS_SCANNING_APP_RESCAN_PAGE_BUTTON_LABEL},
      {"rescanPageConfirmationText",
       IDS_SCANNING_APP_RESCAN_PAGE_CONFIRMATION_TEXT},
      {"resolutionDropdownLabel", IDS_SCANNING_APP_RESOLUTION_DROPDOWN_LABEL},
      {"resolutionOptionText", IDS_SCANNING_APP_RESOLUTION_OPTION_TEXT},
      {"retryButtonLabel", IDS_SCANNING_APP_RETRY_BUTTON_LABEL},
      {"saveButtonLabel", IDS_SCANNING_APP_SAVE_BUTTON_LABEL},
      {"scanCanceledToastText", IDS_SCANNING_APP_SCAN_CANCELED_TOAST_TEXT},
      {"scanFailedDialogAdfEmptyText",
       IDS_SCANNING_APP_SCAN_FAILED_DIALOG_ADF_EMPTY_TEXT},
      {"scanFailedDialogAdfJammedText",
       IDS_SCANNING_APP_SCAN_FAILED_DIALOG_ADF_JAMMED_TEXT},
      {"scanFailedDialogDeviceBusyText",
       IDS_SCANNING_APP_SCAN_FAILED_DIALOG_DEVICE_BUSY_TEXT},
      {"scanFailedDialogFlatbedOpenText",
       IDS_SCANNING_APP_SCAN_FAILED_DIALOG_FLATBED_OPEN_TEXT},
      {"scanFailedDialogIoErrorText",
       IDS_SCANNING_APP_SCAN_FAILED_DIALOG_IO_ERROR_TEXT},
      {"scanFailedDialogTitleText",
       IDS_SCANNING_APP_SCAN_FAILED_DIALOG_TITLE_TEXT},
      {"scanFailedDialogUnknownErrorText",
       IDS_SCANNING_APP_SCAN_FAILED_DIALOG_UNKNOWN_ERROR_TEXT},
      {"scanPreviewHelperText", IDS_SCANNING_APP_SCAN_PREVIEW_HELPER_TEXT},
      {"scanPreviewProgressText", IDS_SCANNING_APP_SCAN_PREVIEW_PROGRESS_TEXT},
      {"scanToDropdownLabel", IDS_SCANNING_APP_SCAN_TO_DROPDOWN_LABEL},
      {"scannerDropdownLabel", IDS_SCANNING_APP_SCANNER_DROPDOWN_LABEL},
      {"scannersLoadingText", IDS_SCANNING_APP_SCANNERS_LOADING_TEXT},
      {"scanningImagesAriaLabel", IDS_SCANNING_APP_SCANNING_IMAGES_ARIA_LABEL},
      {"selectFolderOption", IDS_SCANNING_APP_SELECT_FOLDER_OPTION},
      {"showInFolderButtonLabel", IDS_SCANNING_APP_SHOW_IN_FOLDER_BUTTON_LABEL},
      {"sourceDropdownLabel", IDS_SCANNING_APP_SOURCE_DROPDOWN_LABEL},
      {"startScanFailedToast", IDS_SCANNING_APP_START_SCAN_FAILED_TOAST},
      {"tabloidOptionText", IDS_SCANNING_APP_TABLOID_OPTION_TEXT},
      {"twoSidedDocFeederOptionText",
       IDS_SCANNING_APP_TWO_SIDED_DOC_FEEDER_OPTION_TEXT}};

  html_source->AddLocalizedStrings(kLocalizedStrings);
  html_source->UseStringsJs();
}

void AddScanningAppPluralStrings(ScanningHandler* handler) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"editButtonLabel", IDS_SCANNING_APP_EDIT_BUTTON_LABEL},
      {"fileSavedText", IDS_SCANNING_APP_FILE_SAVED_TEXT},
      {"removePageDialogTitle", IDS_SCANNING_APP_REMOVE_PAGE_DIALOG_TITLE},
      {"rescanPageDialogTitle", IDS_SCANNING_APP_RESCAN_PAGE_DIALOG_TITLE},
      {"scanButtonText", IDS_SCANNING_APP_SCAN_BUTTON_TEXT},
      {"scannedImagesAriaLabel", IDS_SCANNING_APP_SCANNED_IMAGES_ARIA_LABEL}};

  for (const auto& str : kLocalizedStrings)
    handler->AddStringToPluralMap(str.name, str.id);
}

}  // namespace

ScanningUI::ScanningUI(
    content::WebUI* web_ui,
    std::unique_ptr<ScanningAppDelegate> scanning_app_delegate)
    : ui::MojoWebUIController(web_ui, true /* enable_chrome_send */),
      bind_pending_receiver_callback_(
          scanning_app_delegate->GetBindScanServiceCallback(web_ui)) {
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(
          web_ui->GetWebContents()->GetBrowserContext(),
          kChromeUIScanningAppHost);
  html_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://webui-test 'self';");
  ash::EnableTrustedTypesCSP(html_source);

  accessibility_features_ = std::make_unique<AccessibilityFeatures>();

  const auto resources =
      base::make_span(kAshScanningAppResources, kAshScanningAppResourcesSize);
  SetUpWebUIDataSource(html_source, resources, IDR_ASH_SCANNING_APP_INDEX_HTML);

  AddScanningAppStrings(html_source);

  auto handler =
      std::make_unique<ScanningHandler>(std::move(scanning_app_delegate));
  AddScanningAppPluralStrings(handler.get());

  web_ui->AddMessageHandler(std::move(handler));
  web_ui->AddMessageHandler(std::make_unique<ScanningMetricsHandler>());
}

ScanningUI::~ScanningUI() = default;

void ScanningUI::BindInterface(
    mojo::PendingReceiver<scanning::mojom::ScanService> pending_receiver) {
  bind_pending_receiver_callback_.Run(std::move(pending_receiver));
}

void ScanningUI::BindInterface(
    mojo::PendingReceiver<common::mojom::AccessibilityFeatures>
        pending_receiver) {
  accessibility_features_->BindInterface(std::move(pending_receiver));
}

void ScanningUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler> receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(ScanningUI)

}  // namespace ash
