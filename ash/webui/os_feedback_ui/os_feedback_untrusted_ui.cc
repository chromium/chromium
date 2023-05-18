// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/os_feedback_ui/os_feedback_untrusted_ui.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/webui/grit/ash_os_feedback_resources.h"
#include "ash/webui/grit/ash_os_feedback_untrusted_resources.h"
#include "ash/webui/grit/ash_os_feedback_untrusted_resources_map.h"
#include "ash/webui/os_feedback_ui/url_constants.h"
#include "base/containers/span.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "ui/webui/color_change_listener/color_change_handler.h"
#include "url/gurl.h"

namespace ash {
namespace feedback {

namespace {

void AddLocalizedStrings(content::WebUIDataSource* source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"suggestedHelpContent", IDS_FEEDBACK_TOOL_SUGGESTED_HELP_CONTENT},
      {"popularHelpContent", IDS_FEEDBACK_TOOL_POPULAR_HELP_CONTENT},
      {"noMatchedResults", IDS_FEEDBACK_TOOL_NO_MATCHED_RESULTS},
      {"helpContentOfflineMessage",
       IDS_FEEDBACK_TOOL_HELP_CONTENT_OFFLINE_MESSAGE},
      {"helpContentOfflineAltText",
       IDS_FEEDBACK_TOOL_HELP_CONTENT_OFFLINE_ALT_TEXT},
      {"helpContentLabelTooltip", IDS_FEEDBACK_TOOL_HELP_CONTENT_LABEL_TOOLTIP},
      {"helpContentNotAvailableMessage",
       IDS_FEEDBACK_TOOL_HELP_CONTENT_NOT_AVAILABLE_MESSAGE},
      {"helpContentNotAvailableAltText",
       IDS_FEEDBACK_TOOL_HELP_CONTENT_NOT_AVAILABLE_ALT_TEXT},
  };

  source->AddLocalizedStrings(kLocalizedStrings);
  source->UseStringsJs();
}

}  // namespace

OsFeedbackUntrustedUIConfig::OsFeedbackUntrustedUIConfig()
    : WebUIConfig(content::kChromeUIUntrustedScheme,
                  kChromeUIOSFeedbackUntrustedHost) {}

OsFeedbackUntrustedUIConfig::~OsFeedbackUntrustedUIConfig() = default;

bool OsFeedbackUntrustedUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(features::kOsFeedback);
}

std::unique_ptr<content::WebUIController>
OsFeedbackUntrustedUIConfig::CreateWebUIController(content::WebUI* web_ui,
                                                   const GURL& url) {
  return std::make_unique<OsFeedbackUntrustedUI>(web_ui);
}

OsFeedbackUntrustedUI::OsFeedbackUntrustedUI(content::WebUI* web_ui)
    : ui::UntrustedWebUIController(web_ui) {
  content::WebUIDataSource* untrusted_source =
      content::WebUIDataSource::CreateAndAdd(
          web_ui->GetWebContents()->GetBrowserContext(),
          kChromeUIOSFeedbackUntrustedUrl);

  untrusted_source->AddResourcePaths(base::make_span(
      kAshOsFeedbackUntrustedResources, kAshOsFeedbackUntrustedResourcesSize));
  untrusted_source->AddResourcePath("help_content.js",
                                    IDR_ASH_OS_FEEDBACK_HELP_CONTENT_JS);
  untrusted_source->AddResourcePath("feedback_types.js",
                                    IDR_ASH_OS_FEEDBACK_FEEDBACK_TYPES_JS);
  untrusted_source->AddResourcePath(
      "file_path.mojom-lite.js",
      IDR_ASH_OS_FEEDBACK_MOJO_PUBLIC_MOJOM_BASE_FILE_PATH_MOJOM_LITE_JS);
  untrusted_source->AddResourcePath(
      "safe_base_name.mojom-lite.js",
      IDR_ASH_OS_FEEDBACK_MOJO_PUBLIC_MOJOM_BASE_SAFE_BASE_NAME_MOJOM_LITE_JS);
  untrusted_source->AddResourcePath(
      "help_resources_icons.js", IDR_ASH_OS_FEEDBACK_HELP_RESOURCES_ICONS_JS);
  untrusted_source->AddResourcePath(
      "mojom/os_feedback_ui.mojom-lite.js",
      IDR_ASH_OS_FEEDBACK_MOJOM_OS_FEEDBACK_UI_MOJOM_LITE_JS);

  untrusted_source->SetDefaultResource(
      IDR_ASH_OS_FEEDBACK_UNTRUSTED_UNTRUSTED_INDEX_HTML);

  // Resources for dynamic colors.
  untrusted_source->AddBoolean("isJellyEnabledForOsFeedback",
                               ash::features::IsJellyEnabledForOsFeedback());

  AddLocalizedStrings(untrusted_source);

  // Allow the chrome://os-feedback WebUI to embed the corresponding
  // chrome-untrusted://os-feedback WebUI.
  untrusted_source->AddFrameAncestor(GURL(kChromeUIOSFeedbackUrl));

  // DisableTrustedTypesCSP to support TrustedTypePolicy named 'goog#html'.
  // It is the Closure templating system that renders our UI, as it does many
  // other web apps using it.
  untrusted_source->DisableTrustedTypesCSP();
  // TODO(b/194964287): Audit and tighten CSP.
  untrusted_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::DefaultSrc, "");

  untrusted_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src 'self' chrome-untrusted://resources;");
}

OsFeedbackUntrustedUI::~OsFeedbackUntrustedUI() = default;

void OsFeedbackUntrustedUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler> receiver) {
  CHECK(ash::features::IsJellyEnabledForOsFeedback());
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(OsFeedbackUntrustedUI)
}  // namespace feedback
}  // namespace ash
