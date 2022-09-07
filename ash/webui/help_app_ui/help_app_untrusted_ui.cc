// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

namespace ash {

namespace {

content::WebUIDataSource* CreateHelpAppUntrustedDataSource(
    base::RepeatingCallback<void(content::WebUIDataSource*)>
        populate_load_time_data_callback) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(kChromeUIHelpAppUntrustedURL);
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
  return source;
}

}  // namespace

HelpAppUntrustedUI::HelpAppUntrustedUI(
    content::WebUI* web_ui,
    base::RepeatingCallback<void(content::WebUIDataSource* source)>
        populate_load_time_data_callback)
    : ui::UntrustedWebUIController(web_ui) {
  content::WebUIDataSource* untrusted_source =
      CreateHelpAppUntrustedDataSource(populate_load_time_data_callback);

  auto* browser_context = web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource::Add(browser_context, untrusted_source);
}

HelpAppUntrustedUI::~HelpAppUntrustedUI() = default;

}  // namespace ash
