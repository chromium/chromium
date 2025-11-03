// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pdf/pdf_help_bubble_handler_factory.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/features.h"
#include "base/functional/bind.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "components/user_education/webui/help_bubble_handler.h"
#include "content/public/browser/document_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/constants.h"
#include "pdf/pdf_features.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/webui/resources/cr_components/help_bubble/help_bubble.mojom.h"

namespace pdf {

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PdfHelpBubbleHandlerFactory,
                                      kPdfInkSignaturesDrawElementId);

// static
void PdfHelpBubbleHandlerFactory::Create(
    content::RenderFrameHost* render_frame_host,
    HelpFactoryPendingReceiver receiver) {
  if (!base::FeatureList::IsEnabled(chrome_pdf::features::kPdfInk2)) {
    return;
  }

  const GURL& url = render_frame_host->GetLastCommittedURL();
  if (!url.SchemeIs(extensions::kExtensionScheme) ||
      url.host() != extension_misc::kPdfExtensionId) {
    return;
  }

  // This class inherits from content::DocumentService<>, so its lifetime is
  // bound to the associated `render_frame_host`.
  new PdfHelpBubbleHandlerFactory(render_frame_host, std::move(receiver));
}

PdfHelpBubbleHandlerFactory::PdfHelpBubbleHandlerFactory(
    content::RenderFrameHost* render_frame_host,
    HelpFactoryPendingReceiver receiver)
    : DocumentService(*render_frame_host, std::move(receiver)) {}

PdfHelpBubbleHandlerFactory::~PdfHelpBubbleHandlerFactory() = default;

void PdfHelpBubbleHandlerFactory::CreateHelpBubbleHandler(
    mojo::PendingRemote<help_bubble::mojom::HelpBubbleClient> client,
    mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandler> handler) {
  // Normally, `WebUIController` implements `HelpBubbleHandlerFactory`. However,
  // the PDF Viewer does not have a `WebUIController`, so this class uses a
  // custom `HelpBubbleHandler::GetWebContentsCallback` instead.
  auto pdf_get_web_contents_callback = base::BindRepeating(
      [](content::WebContents* web_contents) { return web_contents; },
      content::WebContents::FromRenderFrameHost(&render_frame_host()));
  help_bubble_handler_ = std::make_unique<user_education::HelpBubbleHandler>(
      std::move(handler), std::move(client),
      std::move(pdf_get_web_contents_callback), this,
      std::vector<ui::ElementIdentifier>{kPdfInkSignaturesDrawElementId});
}

}  // namespace pdf
