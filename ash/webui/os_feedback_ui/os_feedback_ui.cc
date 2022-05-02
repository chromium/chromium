// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/os_feedback_ui/os_feedback_ui.h"

#include <memory>
#include <utility>

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
#include "ui/resources/grit/webui_generated_resources.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/webui_allowlist.h"

namespace ash {

namespace {

void SetUpWebUIDataSource(content::WebUIDataSource* source,
                          base::span<const webui::ResourcePath> resources,
                          int default_resource) {
  source->AddResourcePaths(resources);
  source->SetDefaultResource(default_resource);
  source->AddResourcePath("test_loader.html", IDR_WEBUI_HTML_TEST_LOADER_HTML);
  source->AddResourcePath("test_loader.js", IDR_WEBUI_JS_TEST_LOADER_JS);
  source->AddResourcePath("test_loader_util.js",
                          IDR_WEBUI_JS_TEST_LOADER_UTIL_JS);
}

void AddLocalizedStrings(content::WebUIDataSource* source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"continueButtonLabel", IDS_FEEDBACK_TOOL_CONTINUE_BUTTON_LABEL},
      {"descriptionLabel", IDS_FEEDBACK_TOOL_DESCRIPTION_LABEL},
      {"pageTitle", IDS_FEEDBACK_TOOL_PAGE_TITLE},
      // The help content strings are needed for browser tests.
      {"suggestedHelpContent", IDS_FEEDBACK_TOOL_SUGGESTED_HELP_CONTENT},
      {"popularHelpContent", IDS_FEEDBACK_TOOL_POPULAR_HELP_CONTENT},
      {"noMatchedResults", IDS_FEEDBACK_TOOL_NO_MATCHED_RESULTS},
  };

  source->AddLocalizedStrings(kLocalizedStrings);
  source->UseStringsJs();
}

}  // namespace

OSFeedbackUI::OSFeedbackUI(
    content::WebUI* web_ui,
    std::unique_ptr<OsFeedbackDelegate> feedback_delegate)
    : MojoWebUIController(web_ui) {
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
      "script-src chrome://resources chrome://test chrome://webui-test "
      "'self';");
  source->DisableTrustedTypesCSP();

  const auto resources =
      base::make_span(kAshOsFeedbackResources, kAshOsFeedbackResourcesSize);
  SetUpWebUIDataSource(source, resources, IDR_ASH_OS_FEEDBACK_INDEX_HTML);
  AddLocalizedStrings(source);

  // Register common permissions for chrome-untrusted:// pages.
  // TODO(https://crbug.com/1113568): Remove this after common permissions are
  // granted by default.
  auto* webui_allowlist = WebUIAllowlist::GetOrCreate(browser_context);
  const url::Origin untrusted_origin =
      url::Origin::Create(GURL(kChromeUIOSFeedbackUntrustedUrl));
  webui_allowlist->RegisterAutoGrantedPermission(
      untrusted_origin, ContentSettingsType::JAVASCRIPT);

  help_content_provider_ = std::make_unique<feedback::HelpContentProvider>(
      feedback_delegate->GetApplicationLocale(), browser_context);
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

WEB_UI_CONTROLLER_TYPE_IMPL(OSFeedbackUI)
}  // namespace ash
