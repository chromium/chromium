// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/projector_app/trusted_projector_annotator_ui.h"

#include "ash/public/cpp/projector/projector_annotator_controller.h"
#include "ash/webui/grit/ash_projector_annotator_trusted_resources.h"
#include "ash/webui/grit/ash_projector_annotator_trusted_resources_map.h"
#include "ash/webui/grit/ash_projector_common_resources.h"
#include "ash/webui/grit/ash_projector_common_resources_map.h"
#include "ash/webui/projector_app/annotator_page_handler_impl.h"
#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "url/gurl.h"

namespace ash {

namespace {

void CreateAndAddProjectorAnnotatorHTMLSource(content::WebUI* web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      kChromeUIProjectorAnnotatorHost);

  // TODO(b/216523790): Split trusted annotator resources into a separate
  // bundle.
  source->AddResourcePaths(
      base::make_span(kAshProjectorAnnotatorTrustedResources,
                      kAshProjectorAnnotatorTrustedResourcesSize));
  source->AddResourcePaths(base::make_span(kAshProjectorCommonResources,
                                           kAshProjectorCommonResourcesSize));
  source->AddResourcePath(
      "", IDR_ASH_PROJECTOR_ANNOTATOR_TRUSTED_ANNOTATOR_EMBEDDER_HTML);

  std::string csp =
      std::string("frame-src ") + kChromeUIUntrustedAnnotatorUrl + ";";
  // Allow use of SharedArrayBuffer (required by wasm code in the iframe guest).
  source->OverrideCrossOriginOpenerPolicy("same-origin");
  source->OverrideCrossOriginEmbedderPolicy("require-corp");

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameSrc, csp);

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types polymer-html-literal "
      "polymer-template-event-attribute-policy;");
}

}  // namespace

TrustedProjectorAnnotatorUI::TrustedProjectorAnnotatorUI(
    content::WebUI* web_ui,
    const GURL& url,
    PrefService* pref_service)
    : MojoBubbleWebUIController(web_ui, /*enable_chrome_send=*/false) {
  CreateAndAddProjectorAnnotatorHTMLSource(web_ui);

  // The Annotator and Projector SWA embed contents in a sandboxed
  // chrome-untrusted:// iframe.
  web_ui->AddRequestableScheme(content::kChromeUIUntrustedScheme);
}

TrustedProjectorAnnotatorUI::~TrustedProjectorAnnotatorUI() = default;

void TrustedProjectorAnnotatorUI::BindInterface(
    mojo::PendingReceiver<annotator::mojom::AnnotatorPageHandlerFactory>
        receiver) {
  if (receiver_.is_bound()) {
    receiver_.reset();
  }
  receiver_.Bind(std::move(receiver));
}

void TrustedProjectorAnnotatorUI::Create(
    mojo::PendingReceiver<annotator::mojom::AnnotatorPageHandler>
        annotator_handler,
    mojo::PendingRemote<annotator::mojom::AnnotatorPage> annotator) {
  // Multiple WebUIs (and therefore TrustedProjectorAnnotatorUIs) are created
  // for a single Projector recording session, so a new AnnotatorMessageHandler
  // needs to be created each time and attached to the new WebUI. The new
  // handler is then referenced in ProjectorClientImpl.
  handler_ = std::make_unique<AnnotatorPageHandlerImpl>(
      std::move(annotator_handler), std::move(annotator), web_ui());
}

WEB_UI_CONTROLLER_TYPE_IMPL(TrustedProjectorAnnotatorUI)

}  // namespace ash
