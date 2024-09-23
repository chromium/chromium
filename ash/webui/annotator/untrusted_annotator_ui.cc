// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/webui/annotator/untrusted_annotator_ui.h"

#include "ash/webui/grit/ash_annotator_untrusted_resources.h"
#include "ash/webui/grit/ash_annotator_untrusted_resources_map.h"
#include "ash/webui/grit/ash_projector_common_resources.h"
#include "ash/webui/grit/ash_projector_common_resources_map.h"
#include "ash/webui/media_app_ui/buildflags.h"
#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "ash/webui/annotator/untrusted_annotator_page_handler_impl.h"
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

void CreateAndAddAnnotatorHTMLSource(
    content::WebUI* web_ui,
    UntrustedAnnotatorUIDelegate* delegate) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      kChromeUIUntrustedAnnotatorUrl);

  // TODO(b/216523790): Split untrusted annotator resources into a separate
  // bundle.
  source->AddResourcePaths(
      base::make_span(kAshAnnotatorUntrustedResources,
                      kAshAnnotatorUntrustedResourcesSize));
  source->AddResourcePaths(
      base::make_span(kChromeosProjectorAppBundleResources,
                      kChromeosProjectorAppBundleResourcesSize));
  source->AddResourcePaths(base::make_span(kAshProjectorCommonResources,
                                           kAshProjectorCommonResourcesSize));
  source->AddResourcePath("",
                          IDR_ASH_ANNOTATOR_UNTRUSTED_ANNOTATOR_HTML);

#if BUILDFLAG(ENABLE_CROS_MEDIA_APP)
  // Loads WASM resources shipped to Chromium by chrome://media-app.
  source->AddResourcePath("ink_engine_ink.worker.js",
                          IDR_MEDIA_APP_INK_ENGINE_INK_WORKER_JS);
  source->AddResourcePath("ink_engine_ink.wasm",
                          IDR_MEDIA_APP_INK_ENGINE_INK_WASM);
  source->AddResourcePath("ink.js", IDR_MEDIA_APP_INK_JS);
#endif  // BUILDFLAG(ENABLE_CROS_MEDIA_APP)

  // Provide a list of specific script resources (javascript files and inlined
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

  // Loading WASM in chrome-untrusted://projector-annotator/annotator/ink.js is
  // not compatible with trusted types.
  source->DisableTrustedTypesCSP();
  delegate->PopulateLoadTimeData(source);
  source->UseStringsJs();
}

}  // namespace

UntrustedAnnotatorUI::UntrustedAnnotatorUI(
    content::WebUI* web_ui,
    UntrustedAnnotatorUIDelegate* delegate)
    : UntrustedWebUIController(web_ui) {
  CreateAndAddAnnotatorHTMLSource(web_ui, delegate);
}

UntrustedAnnotatorUI::~UntrustedAnnotatorUI() = default;

void UntrustedAnnotatorUI::BindInterface(
    mojo::PendingReceiver<
        annotator::mojom::UntrustedAnnotatorPageHandlerFactory> factory) {
  if (receiver_.is_bound()) {
    receiver_.reset();
  }
  receiver_.Bind(std::move(factory));
}

void UntrustedAnnotatorUI::Create(
    mojo::PendingReceiver<annotator::mojom::UntrustedAnnotatorPageHandler>
        annotator_handler,
    mojo::PendingRemote<annotator::mojom::UntrustedAnnotatorPage> annotator) {
  handler_ = std::make_unique<UntrustedAnnotatorPageHandlerImpl>(
      std::move(annotator_handler), std::move(annotator), web_ui());
}

WEB_UI_CONTROLLER_TYPE_IMPL(UntrustedAnnotatorUI)

}  // namespace ash
