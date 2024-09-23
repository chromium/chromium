// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/webui/eche_app_ui/untrusted_eche_app_ui.h"

#include "ash/webui/eche_app_ui/url_constants.h"
#include "ash/webui/grit/ash_eche_app_resources.h"
#include "ash/webui/grit/ash_eche_bundle_resources.h"
#include "ash/webui/grit/ash_eche_bundle_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/js/grit/mojo_bindings_resources.h"
#include "url/gurl.h"

namespace ash {
namespace eche_app {

UntrustedEcheAppUIConfig::UntrustedEcheAppUIConfig()
    : DefaultWebUIConfig(content::kChromeUIUntrustedScheme,
                         kChromeUIEcheAppGuestHost) {}

UntrustedEcheAppUIConfig::~UntrustedEcheAppUIConfig() = default;

UntrustedEcheAppUI::UntrustedEcheAppUI(content::WebUI* web_ui)
    : ui::UntrustedWebUIController(web_ui) {
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(
          web_ui->GetWebContents()->GetBrowserContext(),
          kChromeUIEcheAppGuestURL);

  html_source->AddResourcePath("untrusted_index.html",
                               IDR_ASH_ECHE_UNTRUSTED_INDEX_HTML);
  html_source->AddResourcePath("js/app_bundle.js", IDR_ASH_ECHE_APP_BUNDLE_JS);
  html_source->AddResourcePath("assets/app_bundle.css",
                               IDR_ASH_ECHE_APP_BUNDLE_CSS);
  html_source->AddResourcePath("message_pipe.js",
                               IDR_ASH_ECHE_APP_MESSAGE_PIPE_JS);
  html_source->AddResourcePath("message_types.js",
                               IDR_ASH_ECHE_APP_MESSAGE_TYPES_JS);
  html_source->AddResourcePath("receiver.js", IDR_ASH_ECHE_APP_RECEIVER_JS);

  html_source->AddResourcePaths(
      base::make_span(kAshEcheBundleResources, kAshEcheBundleResourcesSize));

  html_source->AddFrameAncestor(GURL(kChromeUIEcheAppURL));

  // DisableTrustedTypesCSP to support TrustedTypePolicy named 'goog#html'.
  // It is the Closure templating system that renders our UI, as it does many
  // other web apps using it.
  html_source->DisableTrustedTypesCSP();
  // TODO(b/194964287): Audit and tighten CSP.
  html_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::DefaultSrc, "");
}

UntrustedEcheAppUI::~UntrustedEcheAppUI() = default;

}  // namespace eche_app
}  // namespace ash
