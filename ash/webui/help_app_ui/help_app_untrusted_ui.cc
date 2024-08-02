// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/webui/help_app_ui/help_app_untrusted_ui.h"

#include "ash/webui/grit/ash_help_app_resources.h"
#include "ash/webui/help_app_ui/url_constants.h"
#include "ash/webui/web_applications/webui_test_prod_util.h"
#include "chromeos/grit/chromeos_help_app_bundle_resources.h"
#include "chromeos/grit/chromeos_help_app_bundle_resources_map.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/webui/color_change_listener/color_change_handler.h"

namespace ash {

namespace {

void CreateAndAddHelpAppUntrustedDataSource(
    content::BrowserContext* browser_context,
    base::RepeatingCallback<void(content::WebUIDataSource*)>
        populate_load_time_data_callback) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      browser_context, kChromeUIHelpAppUntrustedURL);
  // app.html is the default resource because it has routing logic to handle all
  // the other paths.
  source->SetDefaultResource(IDR_HELP_APP_APP_HTML);
  source->AddResourcePath("app_bin.js", IDR_HELP_APP_APP_BIN_JS);
  source->AddResourcePath("receiver.js", IDR_HELP_APP_RECEIVER_JS);
  source->DisableTrustedTypesCSP();

  // Add all resources from chromeos_help_app_bundle.pak.
  source->AddResourcePaths(base::make_span(
      kChromeosHelpAppBundleResources, kChromeosHelpAppBundleResourcesSize));

  MaybeConfigureTestableDataSource(source, "help_app/untrusted");

  // Add device and feature flags.
  populate_load_time_data_callback.Run(source);
  source->AddLocalizedString("appName", IDS_HELP_APP_EXPLORE);

  source->UseStringsJs();
  source->AddFrameAncestor(GURL(kChromeUIHelpAppURL));

  // TODO(https://crbug.com/1085328): Audit and tighten CSP.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::DefaultSrc, "");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ChildSrc,
      "child-src 'self' chrome-untrusted://help-app-kids-magazine;");
}

}  // namespace

HelpAppUntrustedUI::HelpAppUntrustedUI(
    content::WebUI* web_ui,
    base::RepeatingCallback<void(content::WebUIDataSource* source)>
        populate_load_time_data_callback)
    : ui::UntrustedWebUIController(web_ui) {
  CreateAndAddHelpAppUntrustedDataSource(
      web_ui->GetWebContents()->GetBrowserContext(),
      populate_load_time_data_callback);
}

HelpAppUntrustedUI::~HelpAppUntrustedUI() = default;

void HelpAppUntrustedUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler> receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(HelpAppUntrustedUI)
}  // namespace ash
