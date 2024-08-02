// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/webui/sample_system_web_app_ui/sample_system_web_app_ui.h"

#include <utility>

#include "ash/webui/grit/ash_sample_system_web_app_resources.h"
#include "ash/webui/grit/ash_sample_system_web_app_resources_map.h"
#include "ash/webui/sample_system_web_app_ui/sample_page_handler.h"
#include "ash/webui/sample_system_web_app_ui/url_constants.h"
#include "base/base64.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "crypto/random.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/webui/color_change_listener/color_change_handler.h"
#include "ui/webui/webui_allowlist.h"

namespace ash {

SampleSystemWebAppUI::SampleSystemWebAppUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  auto* browser_context = web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource* trusted_source =
      content::WebUIDataSource::CreateAndAdd(browser_context,
                                             kChromeUISampleSystemWebAppHost);
  trusted_source->AddResourcePath("", IDR_ASH_SAMPLE_SYSTEM_WEB_APP_INDEX_HTML);
  trusted_source->AddResourcePaths(base::make_span(
      kAshSampleSystemWebAppResources, kAshSampleSystemWebAppResourcesSize));

#if !DCHECK_IS_ON()
  // If a user goes to an invalid url and non-DCHECK mode (DHECK = debug mode)
  // is set, serve a default page so the user sees your default page instead
  // of an unexpected error. But if DCHECK is set, the user will be a
  // developer and be able to identify an error occurred.
  trusted_source->SetDefaultResource(IDR_ASH_SAMPLE_SYSTEM_WEB_APP_INDEX_HTML);
#endif  // !DCHECK_IS_ON()

  // We need a CSP override to use the chrome-untrusted:// scheme in the host.
  std::string csp =
      std::string("frame-src ") + kChromeUISampleSystemWebAppUntrustedURL + ";";
  trusted_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameSrc, csp);

  trusted_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::WorkerSrc,
      std::string("worker-src 'self';"));
  trusted_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types lit-html worker-js-static;");
  trusted_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://webui-test 'self';");

  // Add ability to request chrome-untrusted: URLs
  web_ui->AddRequestableScheme(content::kChromeUIUntrustedScheme);

  // Register common permissions for chrome-untrusted:// pages.
  // TODO(crbug.com/40710326): Remove this after common permissions are
  // granted by default.
  auto* webui_allowlist = WebUIAllowlist::GetOrCreate(browser_context);
  const url::Origin sample_system_web_app_untrusted_origin =
      url::Origin::Create(GURL(kChromeUISampleSystemWebAppUntrustedURL));
  for (const auto& permission : {
           ContentSettingsType::COOKIES,
           ContentSettingsType::JAVASCRIPT,
           ContentSettingsType::IMAGES,
           ContentSettingsType::SOUND,
       }) {
    webui_allowlist->RegisterAutoGrantedPermission(
        sample_system_web_app_untrusted_origin, permission);
  }
}

SampleSystemWebAppUI::~SampleSystemWebAppUI() = default;

void SampleSystemWebAppUI::BindInterface(
    mojo::PendingReceiver<mojom::sample_swa::PageHandlerFactory> factory) {
  if (sample_page_factory_.is_bound()) {
    sample_page_factory_.reset();
  }
  sample_page_factory_.Bind(std::move(factory));
}

void SampleSystemWebAppUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler> receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(receiver));
}

void SampleSystemWebAppUI::CreatePageHandler(
    mojo::PendingReceiver<mojom::sample_swa::PageHandler> handler,
    mojo::PendingRemote<mojom::sample_swa::Page> page) {
  DCHECK(page.is_valid());
  sample_page_handler_->BindInterface(std::move(handler), std::move(page));
}

void SampleSystemWebAppUI::CreateParentPage(
    mojo::PendingRemote<mojom::sample_swa::ChildUntrustedPage> child_page,
    mojo::PendingReceiver<mojom::sample_swa::ParentTrustedPage> parent_page) {
  sample_page_handler_->CreateParentPage(std::move(child_page),
                                         std::move(parent_page));
}

void SampleSystemWebAppUI::WebUIPrimaryPageChanged(content::Page& page) {
  // Create a new page handler for each document load. This avoids sharing
  // states when WebUIController is reused for same-origin navigations.
  sample_page_handler_ = std::make_unique<PageHandler>();
}

WEB_UI_CONTROLLER_TYPE_IMPL(SampleSystemWebAppUI)

}  // namespace ash
