// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/os_feedback_ui/os_feedback_ui.h"

#include <memory>
#include <utility>

#include "ash/webui/grit/ash_os_feedback_resources.h"
#include "ash/webui/grit/ash_os_feedback_resources_map.h"
#include "ash/webui/os_feedback_ui/backend/help_content_provider.h"
#include "ash/webui/os_feedback_ui/mojom/os_feedback_ui.mojom.h"
#include "ash/webui/os_feedback_ui/url_constants.h"
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

}  // namespace

OSFeedbackUI::OSFeedbackUI(content::WebUI* web_ui)
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

  // Register common permissions for chrome-untrusted:// pages.
  // TODO(https://crbug.com/1113568): Remove this after common permissions are
  // granted by default.
  auto* webui_allowlist = WebUIAllowlist::GetOrCreate(browser_context);
  const url::Origin untrusted_origin =
      url::Origin::Create(GURL(kChromeUIOSFeedbackUntrustedUrl));
  webui_allowlist->RegisterAutoGrantedPermission(
      untrusted_origin, ContentSettingsType::JAVASCRIPT);

  helpContentProvider_ = std::make_unique<feedback::HelpContentProvider>();
}

OSFeedbackUI::~OSFeedbackUI() = default;

void OSFeedbackUI::BindInterface(
    mojo::PendingReceiver<os_feedback_ui::mojom::HelpContentProvider>
        receiver) {
  helpContentProvider_->BindInterface(std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(OSFeedbackUI)
}  // namespace ash
