// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/webui/os_feedback_ui/os_feedback_untrusted_ui.h"

#include <memory>

#include "ash/webui/common/trusted_types_util.h"
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
    : DefaultWebUIConfig(content::kChromeUIUntrustedScheme,
                         kChromeUIOSFeedbackUntrustedHost) {}

OsFeedbackUntrustedUIConfig::~OsFeedbackUntrustedUIConfig() = default;

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
  untrusted_source->AddResourcePath("help_content.html.js",
                                    IDR_ASH_OS_FEEDBACK_HELP_CONTENT_HTML_JS);
  untrusted_source->AddResourcePath("feedback_types.js",
                                    IDR_ASH_OS_FEEDBACK_FEEDBACK_TYPES_JS);
  untrusted_source->AddResourcePath(
      "help_resources_icons.html.js",
      IDR_ASH_OS_FEEDBACK_HELP_RESOURCES_ICONS_HTML_JS);
  untrusted_source->AddResourcePath(
      "os_feedback_ui.mojom-webui.js",
      IDR_ASH_OS_FEEDBACK_OS_FEEDBACK_UI_MOJOM_WEBUI_JS);

  untrusted_source->SetDefaultResource(
      IDR_ASH_OS_FEEDBACK_UNTRUSTED_UNTRUSTED_INDEX_HTML);

  AddLocalizedStrings(untrusted_source);

  // Allow the chrome://os-feedback WebUI to embed the corresponding
  // chrome-untrusted://os-feedback WebUI.
  untrusted_source->AddFrameAncestor(GURL(kChromeUIOSFeedbackUrl));

  ash::EnableTrustedTypesCSP(untrusted_source);
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
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(OsFeedbackUntrustedUI)
}  // namespace feedback
}  // namespace ash
