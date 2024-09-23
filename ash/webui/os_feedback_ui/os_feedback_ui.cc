// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/webui/os_feedback_ui/os_feedback_ui.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/webui/common/trusted_types_util.h"
#include "ash/webui/grit/ash_os_feedback_resources.h"
#include "ash/webui/grit/ash_os_feedback_resources_map.h"
#include "ash/webui/os_feedback_ui/backend/feedback_service_provider.h"
#include "ash/webui/os_feedback_ui/backend/help_content_provider.h"
#include "ash/webui/os_feedback_ui/backend/os_feedback_delegate.h"
#include "ash/webui/os_feedback_ui/mojom/os_feedback_ui.mojom.h"
#include "ash/webui/os_feedback_ui/url_constants.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/resources/grit/webui_resources.h"
#include "ui/web_dialogs/web_dialog_ui.h"
#include "ui/webui/color_change_listener/color_change_handler.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/webui_allowlist.h"

namespace ash {

namespace {

void SetUpWebUIDataSource(content::WebUIDataSource* source,
                          base::span<const webui::ResourcePath> resources,
                          int default_resource) {
  source->AddResourcePaths(resources);
  source->SetDefaultResource(default_resource);
  source->AddResourcePath("test_loader.html", IDR_WEBUI_TEST_LOADER_HTML);
  source->AddResourcePath("test_loader.js", IDR_WEBUI_JS_TEST_LOADER_JS);
  source->AddResourcePath("test_loader_util.js",
                          IDR_WEBUI_JS_TEST_LOADER_UTIL_JS);
}

void AddLocalizedStrings(content::WebUIDataSource* source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"backButtonLabel", IDS_FEEDBACK_TOOL_BACK_BUTTON_LABEL},
      {"dialogBackButtonAriaLabel",
       IDS_FEEDBACK_TOOL_DIALOG_BACK_BUTTON_ARIA_LABEL},
      {"continueButtonLabel", IDS_FEEDBACK_TOOL_CONTINUE_BUTTON_LABEL},
      {"descriptionHint", IDS_FEEDBACK_TOOL_DESCRIPTION_HINT},
      {"descriptionLabel", IDS_FEEDBACK_TOOL_DESCRIPTION_LABEL},
      {"descriptionRequired", IDS_FEEDBACK_TOOL_DESCRIPTION_REQUIRED},
      {"feedbackHelpLinkLabel", IDS_FEEDBACK_TOOL_FEEDBACK_HELP_LINK_LABEL},
      {"pageTitle", IDS_FEEDBACK_TOOL_PAGE_TITLE},
      {"privacyNote", IDS_FEEDBACK_TOOL_PRIVACY_NOTE},
      {"privacyNoteLoggedOut", IDS_FEEDBACK_TOOL_PRIVACY_NOTE_LOGGED_OUT},
      {"mayBeShareWithPartnerNote", IDS_FEEDBACK_TOOL_MAY_BE_SHARED_NOTE},
      {"sendButtonLabel", IDS_FEEDBACK_TOOL_SEND_BUTTON_LABEL},
      // The help content strings are needed for browser tests.
      {"suggestedHelpContent", IDS_FEEDBACK_TOOL_SUGGESTED_HELP_CONTENT},
      {"popularHelpContent", IDS_FEEDBACK_TOOL_POPULAR_HELP_CONTENT},
      {"helpContentOfflineMessage",
       IDS_FEEDBACK_TOOL_HELP_CONTENT_OFFLINE_MESSAGE},
      {"helpContentOfflineAltText",
       IDS_FEEDBACK_TOOL_HELP_CONTENT_OFFLINE_ALT_TEXT},
      {"helpContentLabelTooltip", IDS_FEEDBACK_TOOL_HELP_CONTENT_LABEL_TOOLTIP},
      {"helpContentNotAvailableMessage",
       IDS_FEEDBACK_TOOL_HELP_CONTENT_NOT_AVAILABLE_MESSAGE},
      {"helpContentNotAvailableAltText",
       IDS_FEEDBACK_TOOL_HELP_CONTENT_NOT_AVAILABLE_ALT_TEXT},
      {"noMatchedResults", IDS_FEEDBACK_TOOL_NO_MATCHED_RESULTS},
      {"attachFilesLabelLoggedIn", IDS_FEEDBACK_TOOL_ATTACH_FILES_LABEL},
      {"attachFilesLabelLoggedOut",
       IDS_FEEDBACK_TOOL_ATTACH_FILES_LABEL_LOGGED_OUT},
      {"attachScreenshotLabel", IDS_FEEDBACK_TOOL_SCREENSHOT_LABEL},
      {"previewScreenshotDialogLabel",
       IDS_FEEDBACK_TOOL_PREVIEW_SCREENSHOT_DIALOG_LABEL},
      {"attachScreenshotCheckboxAriaLabel",
       IDS_FEEDBACK_TOOL_ATTACH_SCREENSHOT_CHECKBOX_ARIA_LABEL},
      {"previewImageAriaLabel", IDS_FEEDBACK_TOOL_PREVIEW_IMAGE_ARIA_LABEL},
      {"previewImageDialogLabel", IDS_FEEDBACK_TOOL_PREVIEW_IMAGE_DIALOG_LABEL},
      {"addFileLabel", IDS_FEEDBACK_TOOL_ADD_FILE_LABEL},
      {"replaceFileLabel", IDS_FEEDBACK_TOOL_REPLACE_FILE_LABEL},
      {"attachFileLabelTooltip", IDS_FEEDBACK_TOOL_ATTACH_FILE_LABEL_TOOLTIP},
      {"attachFileCheckboxArialLabel",
       IDS_FEEDBACK_TOOL_ATTACH_FILE_CHECKBOX_ARIA_LABEL},
      {"userEmailLabel", IDS_FEEDBACK_TOOL_USER_EMAIL_LABEL},
      {"userEmailAriaLabel", IDS_FEEDBACK_TOOL_USER_EMAIL_ARIA_LABEL},
      {"shareDiagnosticDataLabel",
       IDS_FEEDBACK_TOOL_SHARE_DIAGNOSTIC_DATA_LABEL},
      {"sharePageUrlLabel", IDS_FEEDBACK_TOOL_SHARE_PAGE_URL_LABEL},
      {"confirmationTitleOnline", IDS_FEEDBACK_TOOL_PAGE_TITLE_AFTER_SENT},
      {"confirmationTitleOffline", IDS_FEEDBACK_TOOL_PAGE_TITLE_SEND_OFFLINE},
      {"exploreAppDescription",
       IDS_FEEDBACK_TOOL_RESOURCES_EXPLORE_APP_DESCRIPTION},
      {"exploreAppLabel", IDS_FEEDBACK_TOOL_RESOURCES_EXPLORE_APP_LABEL},
      {"diagnosticsAppLabel",
       IDS_FEEDBACK_TOOL_RESOURCES_DIAGNOSTICS_APP_LABEL},
      {"diagnosticsAppDescription",
       IDS_FEEDBACK_TOOL_RESOURCES_DIAGNOSTICS_APP_DESCRIPTION},
      {"askCommunityLabel", IDS_FEEDBACK_TOOL_RESOURCES_ASK_COMMUNITY_LABEL},
      {"askCommunityDescription",
       IDS_FEEDBACK_TOOL_RESOURCES_ASK_COMMUNITY_DESCRIPTION},
      {"userConsentLabel", IDS_FEEDBACK_TOOL_USER_CONSENT_LABEL},
      {"includeSystemInfoAndMetricsCheckboxLabel",
       IDS_FEEDBACK_TOOL_INCLUDE_SYSTEM_INFO_AND_METRICS_CHECKBOX_LABEL},
      {"includePerformanceTraceCheckboxLabel",
       IDS_FEEDBACK_TOOL_INCLUDE_PERFORMANCE_TRACE_CHECKBOX_LABEL},
      {"anonymousUser", IDS_FEEDBACK_TOOL_ANONYMOUS_EMAIL_OPTION},
      {"thankYouNoteOffline", IDS_FEEDBACK_TOOL_THANK_YOU_NOTE_OFFLINE},
      {"thankYouNoteOnline", IDS_FEEDBACK_TOOL_THANK_YOU_NOTE_ONLINE},
      {"helpResourcesLabel", IDS_FEEDBACK_TOOL_HELP_RESOURCES_LABEL},
      {"buttonNewReport", IDS_FEEDBACK_TOOL_SEND_NEW_REPORT_BUTTON_LABEL},
      {"buttonDone", IDS_FEEDBACK_TOOL_DONE_BUTTON_LABEL},
      {"fileTooBigErrorMessage", IDS_FEEDBACK_TOOL_FILE_TOO_BIG_ERROR_MESSAGE},
      {"bluetoothLogsInfo", IDS_FEEDBACK_TOOL_BLUETOOTH_LOGS_CHECKBOX},
      {"bluetoothLogsMessage", IDS_FEEDBACK_TOOL_BLUETOOTH_LOGS_MESSAGE},
      {"wifiDebugLogsInfo", IDS_FEEDBACK_TOOL_WIFI_DEBUG_LOGS_CHECKBOX},
      {"wifiDebugLogsMessage", IDS_FEEDBACK_TOOL_WIFI_DEBUG_LOGS_MESSAGE},
      {"wifiDebugLogsTitle", IDS_FEEDBACK_TOOL_WIFI_DEBUG_LOGS_TITLE},
      {"linkCrossDeviceDogfoodFeedbackInfo",
       IDS_FEEDBACK_TOOL_LINK_CROSS_DEVICE_DOGFOOD_FEEDBACK_INFO},
      {"linkCrossDeviceDogfoodFeedbackMessage",
       IDS_FEEDBACK_TOOL_LINK_CROSS_DEVICE_DOGFOOD_FEEDBACK_MESSAGE},
      {"includeAssistantLogsCheckboxLabel",
       IDS_FEEDBACK_TOOL_ASSISTANT_LOGS_CHECKBOX},
      {"assistantLogsMessage", IDS_FEEDBACK_TOOL_ASSISTANT_LOGS_MESSAGE},
      {"includeAutofillCheckboxLabel",
       IDS_FEEDBACK_TOOL_AUTOFILL_LOGS_CHECKBOX},
  };

  source->AddLocalizedStrings(kLocalizedStrings);
  source->UseStringsJs();

  source->AddBoolean("enableLinkCrossDeviceDogfoodFeedbackFlag",
                     features::IsLinkCrossDeviceDogfoodFeedbackEnabled());
}

}  // namespace

OSFeedbackUI::OSFeedbackUI(
    content::WebUI* web_ui,
    std::unique_ptr<OsFeedbackDelegate> feedback_delegate)
    : ui::MojoWebDialogUI(web_ui) {
  auto* browser_context = web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      browser_context, kChromeUIOSFeedbackHost);

  // Add ability to request chrome-untrusted://os-feedback URLs.
  web_ui->AddRequestableScheme(content::kChromeUIUntrustedScheme);
  // We need a CSP override to use the chrome-untrusted:// scheme in the host.
  const std::string csp =
      std::string("frame-src ") + kChromeUIOSFeedbackUntrustedUrl + ";";
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameSrc, csp);

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://webui-test 'self';");
  ash::EnableTrustedTypesCSP(source);

  const auto resources =
      base::make_span(kAshOsFeedbackResources, kAshOsFeedbackResourcesSize);
  SetUpWebUIDataSource(source, resources, IDR_ASH_OS_FEEDBACK_INDEX_HTML);
  AddLocalizedStrings(source);

  // Register common permissions for chrome-untrusted:// pages.
  // TODO(crbug.com/40710326): Remove this after common permissions are
  // granted by default.
  auto* webui_allowlist = WebUIAllowlist::GetOrCreate(browser_context);
  const url::Origin untrusted_origin =
      url::Origin::Create(GURL(kChromeUIOSFeedbackUntrustedUrl));
  webui_allowlist->RegisterAutoGrantedPermission(
      untrusted_origin, ContentSettingsType::JAVASCRIPT);

  help_content_provider_ = std::make_unique<feedback::HelpContentProvider>(
      feedback_delegate->GetApplicationLocale(),
      feedback_delegate->IsChildAccount(), browser_context);
  feedback_service_provider_ =
      std::make_unique<feedback::FeedbackServiceProvider>(
          std::move(feedback_delegate));
}

OSFeedbackUI::~OSFeedbackUI() = default;

void OSFeedbackUI::BindInterface(
    mojo::PendingReceiver<os_feedback_ui::mojom::FeedbackServiceProvider>
        receiver) {
  feedback_service_provider_->BindInterface(std::move(receiver));
}
void OSFeedbackUI::BindInterface(
    mojo::PendingReceiver<os_feedback_ui::mojom::HelpContentProvider>
        receiver) {
  help_content_provider_->BindInterface(std::move(receiver));
}

void OSFeedbackUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler> receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(OSFeedbackUI)
}  // namespace ash
