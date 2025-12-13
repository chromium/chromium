// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PDF_PDF_HELP_BUBBLE_HANDLER_FACTORY_H_
#define CHROME_BROWSER_PDF_PDF_HELP_BUBBLE_HANDLER_FACTORY_H_

#include <memory>

#include "content/public/browser/document_service.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/webui/resources/cr_components/help_bubble/help_bubble.mojom.h"

namespace user_education {
class HelpBubbleHandler;
}

namespace pdf {

// A factory that creates `HelpBubbleHandler` for a given `RenderFrameHost`. Its
// lifetime is bound to the associates `RenderFrameHost`.
class PdfHelpBubbleHandlerFactory
    : public content::DocumentService<
          help_bubble::mojom::PdfHelpBubbleHandlerFactory> {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kPdfInkSignaturesDrawElementId);

  using HelpFactoryPendingReceiver =
      mojo::PendingReceiver<help_bubble::mojom::PdfHelpBubbleHandlerFactory>;

  // Creates the factory only when `render_frame_host` is for the PDF Viewer.
  static void Create(content::RenderFrameHost* render_frame_host,
                     HelpFactoryPendingReceiver receiver);

  PdfHelpBubbleHandlerFactory(const PdfHelpBubbleHandlerFactory&) = delete;
  PdfHelpBubbleHandlerFactory& operator=(const PdfHelpBubbleHandlerFactory&) =
      delete;
  ~PdfHelpBubbleHandlerFactory() override;

  // help_bubble::mojom::PdfHelpBubbleHandlerFactory:
  void CreateHelpBubbleHandler(
      mojo::PendingRemote<help_bubble::mojom::HelpBubbleClient> client,
      mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandler> handler)
      override;

 private:
  PdfHelpBubbleHandlerFactory(content::RenderFrameHost* render_frame_host,
                              HelpFactoryPendingReceiver receiver);

  std::unique_ptr<user_education::HelpBubbleHandler> help_bubble_handler_;
};

}  // namespace pdf

#endif  // CHROME_BROWSER_PDF_PDF_HELP_BUBBLE_HANDLER_FACTORY_H_
