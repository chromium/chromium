// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/projector_app/untrusted_projector_annotator_ui.h"

#include "ash/webui/grit/ash_projector_app_untrusted_resources.h"
#include "ash/webui/grit/ash_projector_app_untrusted_resources_map.h"
#include "ash/webui/media_app_ui/buildflags.h"
#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "chromeos/grit/chromeos_projector_app_bundle_resources.h"
#include "chromeos/grit/chromeos_projector_app_bundle_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_CROS_MEDIA_APP)
#include "chromeos/grit/chromeos_media_app_bundle_resources.h"
#endif  // BUILDFLAG(ENABLE_CROS_MEDIA_APP)

namespace ash {

namespace {

content::WebUIDataSource* CreateProjectorAnnotatorHTMLSource(
    UntrustedProjectorAnnotatorUIDelegate* delegate) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(kChromeUIUntrustedAnnotatorUrl);

  // TODO(b/216523790): Split untrusted annotator resources into a separate
  // bundle.
  source->AddResourcePaths(
      base::make_span(kAshProjectorAppUntrustedResources,
                      kAshProjectorAppUntrustedResourcesSize));
  source->AddResourcePaths(
      base::make_span(kChromeosProjectorAppBundleResources,
                      kChromeosProjectorAppBundleResourcesSize));

#if BUILDFLAG(ENABLE_CROS_MEDIA_APP)
  // Loads WASM resources shipped to Chromium by chrome://media-app.
  source->AddResourcePath("annotator/ink_engine_ink.worker.js",
                          IDR_MEDIA_APP_INK_ENGINE_INK_WORKER_JS);
  source->AddResourcePath("annotator/ink_engine_ink.wasm",
                          IDR_MEDIA_APP_INK_ENGINE_INK_WASM);
  source->AddResourcePath("annotator/ink.js", IDR_MEDIA_APP_INK_JS);
#endif  // BUILDFLAG(ENABLE_CROS_MEDIA_APP)

  // Provide a list of specific script resources(javascript files and inlined
  // scripts inside html) or their sha-256 hashes to allow to be executed.
  // "wasm-eval" is added to allow wasm.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src 'self' 'wasm-eval' chrome-untrusted://resources;");
  // Need to explicitly set |worker-src| because CSP falls back to |child-src|
  // which is none.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::WorkerSrc, "worker-src 'self';");
  // Allow styles to include inline styling needed for Polymer elements.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::StyleSrc,
      "style-src 'self' 'unsafe-inline';");
  // Allow images to also handle data urls.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ImgSrc, "img-src blob: data: 'self';");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ConnectSrc, "connect-src 'self';");

  // Allow use of SharedArrayBuffer (required by the wasm).
  source->OverrideCrossOriginOpenerPolicy("same-origin");
  source->OverrideCrossOriginEmbedderPolicy("require-corp");

  // chrome://projector-annotator and chrome-untrusted://projector-annotator are
  // different origins, so allow resources in the untrusted origin to be loaded
  // cross-origin.
  source->OverrideCrossOriginResourcePolicy("cross-origin");

  source->DisableTrustedTypesCSP();

  source->AddFrameAncestor(GURL(kChromeUITrustedAnnotatorUrl));

  delegate->PopulateLoadTimeData(source);
  source->UseStringsJs();

  return source;
}

}  // namespace

UntrustedProjectorAnnotatorUI::UntrustedProjectorAnnotatorUI(
    content::WebUI* web_ui,
    UntrustedProjectorAnnotatorUIDelegate* delegate)
    : UntrustedWebUIController(web_ui) {
  auto* browser_context = web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource::Add(browser_context,
                                CreateProjectorAnnotatorHTMLSource(delegate));
}

UntrustedProjectorAnnotatorUI::~UntrustedProjectorAnnotatorUI() = default;

}  // namespace ash
