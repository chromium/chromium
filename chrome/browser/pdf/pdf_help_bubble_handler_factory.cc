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
#include "chrome/common/chrome_features.h"
#include "components/user_education/webui/help_bubble_handler.h"
#include "content/public/browser/document_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/constants.h"
#include "pdf/buildflags.h"
#include "pdf/pdf_features.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/webui/resources/cr_components/help_bubble/help_bubble.mojom.h"
#include "ui/webui/tracked_element/tracked_element_handler.h"

namespace pdf {

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PdfHelpBubbleHandlerFactory,
                                      kPdfGlicSummarizeElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PdfHelpBubbleHandlerFactory,
                                      kPdfInkSignaturesDrawElementId);

// static
void PdfHelpBubbleHandlerFactory::Create(
    content::RenderFrameHost* render_frame_host,
    HelpFactoryPendingReceiver receiver) {
  bool is_feature_enabled =
      base::FeatureList::IsEnabled(features::kPdfGlicSummarize);
#if BUILDFLAG(ENABLE_PDF_INK2)
  is_feature_enabled |=
      base::FeatureList::IsEnabled(chrome_pdf::features::kPdfInk2);
#endif  // BUILDFLAG(ENABLE_PDF_INK2)

  if (!is_feature_enabled) {
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
    mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandler>
        help_bubble_handler,
    mojo::PendingReceiver<tracked_element::mojom::TrackedElementHandler>
        tracked_element_handler) {
  tracked_element_handler_ = std::make_unique<ui::TrackedElementHandler>(
      content::WebContents::FromRenderFrameHost(&render_frame_host()),
      ui::ElementContext(this, base::PassKey<PdfHelpBubbleHandlerFactory>()),
      std::vector<ui::ElementIdentifier>{
          PdfHelpBubbleHandlerFactory::kPdfGlicSummarizeElementId,
          PdfHelpBubbleHandlerFactory::kPdfInkSignaturesDrawElementId});
  tracked_element_handler_->BindInterface(std::move(tracked_element_handler));
  help_bubble_handler_ = std::make_unique<user_education::HelpBubbleHandler>(
      std::move(help_bubble_handler), std::move(client),
      tracked_element_handler_->GetWeakPtr());
}

}  // namespace pdf
