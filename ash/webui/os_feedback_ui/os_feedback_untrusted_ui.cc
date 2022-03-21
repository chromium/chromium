// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/os_feedback_ui/os_feedback_untrusted_ui.h"

#include <memory>

#include "ash/webui/grit/ash_os_feedback_resources.h"
#include "ash/webui/grit/ash_os_feedback_untrusted_resources.h"
#include "ash/webui/grit/ash_os_feedback_untrusted_resources_map.h"
#include "ash/webui/os_feedback_ui/url_constants.h"
#include "base/containers/span.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"

namespace ash {
namespace feedback {

OsFeedbackUntrustedUIConfig::OsFeedbackUntrustedUIConfig()
    : WebUIConfig(content::kChromeUIUntrustedScheme,
                  kChromeUIOSFeedbackUntrustedHost) {}

OsFeedbackUntrustedUIConfig::~OsFeedbackUntrustedUIConfig() = default;

std::unique_ptr<content::WebUIController>
OsFeedbackUntrustedUIConfig::CreateWebUIController(content::WebUI* web_ui) {
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
      "help_resources_icons.js", IDR_ASH_OS_FEEDBACK_HELP_RESOURCES_ICONS_JS);
  untrusted_source->AddResourcePath(
      "mojom/os_feedback_ui.mojom-lite.js",
      IDR_ASH_OS_FEEDBACK_MOJOM_OS_FEEDBACK_UI_MOJOM_LITE_JS);

  untrusted_source->SetDefaultResource(
      IDR_ASH_OS_FEEDBACK_UNTRUSTED_UNTRUSTED_INDEX_HTML);

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

}  // namespace feedback
}  // namespace ash
