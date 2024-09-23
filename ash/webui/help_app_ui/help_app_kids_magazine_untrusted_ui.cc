// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/webui/help_app_ui/help_app_kids_magazine_untrusted_ui.h"

#include <string_view>

#include "ash/webui/help_app_ui/url_constants.h"
#include "chromeos/grit/chromeos_help_app_kids_magazine_bundle_resources.h"
#include "chromeos/grit/chromeos_help_app_kids_magazine_bundle_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"

namespace ash {

namespace {

const char kKidsMagazinePathPrefix[] = "kids_magazine/";

// Function to remove a prefix from an input string. Does nothing if the string
// does not begin with the prefix.
std::string_view StripPrefix(std::string_view input, std::string_view prefix) {
  if (input.find(prefix) == 0) {
    return input.substr(prefix.size());
  }
  return input;
}

void CreateAndAddHelpAppKidsMagazineUntrustedDataSource(
    content::WebUI* web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      kChromeUIHelpAppKidsMagazineUntrustedURL);
  // Set index.html as the default resource.
  source->SetDefaultResource(IDR_HELP_APP_KIDS_MAGAZINE_INDEX_HTML);
  source->DisableTrustedTypesCSP();

  for (size_t i = 0; i < kChromeosHelpAppKidsMagazineBundleResourcesSize; i++) {
    // While the JS and CSS file are stored in /kids_magazine/static/..., the
    // HTML file references /static/... directly. We need to strip the
    // "kids_magazine" prefix from the path.
    source->AddResourcePath(
        StripPrefix(kChromeosHelpAppKidsMagazineBundleResources[i].path,
                    kKidsMagazinePathPrefix),
        kChromeosHelpAppKidsMagazineBundleResources[i].id);
  }

  // Add chrome://help-app and chrome-untrusted://help-app as frame ancestors.
  source->AddFrameAncestor(GURL(kChromeUIHelpAppURL));
  source->AddFrameAncestor(GURL(kChromeUIHelpAppUntrustedURL));
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::DefaultSrc, "");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src 'self' https://www.gstatic.com;");
}

}  // namespace

HelpAppKidsMagazineUntrustedUIConfig::HelpAppKidsMagazineUntrustedUIConfig()
    : DefaultWebUIConfig(content::kChromeUIUntrustedScheme,
                         kChromeUIHelpAppKidsMagazineHost) {}

HelpAppKidsMagazineUntrustedUIConfig::~HelpAppKidsMagazineUntrustedUIConfig() =
    default;

HelpAppKidsMagazineUntrustedUI::HelpAppKidsMagazineUntrustedUI(
    content::WebUI* web_ui)
    : ui::UntrustedWebUIController(web_ui) {
  CreateAndAddHelpAppKidsMagazineUntrustedDataSource(web_ui);
}

HelpAppKidsMagazineUntrustedUI::~HelpAppKidsMagazineUntrustedUI() = default;

}  // namespace ash
