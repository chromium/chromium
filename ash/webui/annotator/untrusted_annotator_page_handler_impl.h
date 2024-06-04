// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ANNOTATOR_UNTRUSTED_ANNOTATOR_PAGE_HANDLER_IMPL_H_
#define ASH_WEBUI_ANNOTATOR_UNTRUSTED_ANNOTATOR_PAGE_HANDLER_IMPL_H_

#include "ash/webui/annotator/mojom/untrusted_annotator.mojom.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class WebUI;
}  // namespace content

namespace ash {

struct AnnotatorTool;

// Handles communication with the Annotator WebUI (i.e.
// chrome-untrusted://projector/annotator/annotator_embedder.html)
class UntrustedAnnotatorPageHandlerImpl
    : public annotator::mojom::UntrustedAnnotatorPageHandler {
 public:
  UntrustedAnnotatorPageHandlerImpl(
      mojo::PendingReceiver<annotator::mojom::UntrustedAnnotatorPageHandler>
          annotator_handler,
      mojo::PendingRemote<annotator::mojom::UntrustedAnnotatorPage> annotator,
      content::WebUI* web_ui);
  UntrustedAnnotatorPageHandlerImpl(const UntrustedAnnotatorPageHandlerImpl&) =
      delete;
  UntrustedAnnotatorPageHandlerImpl& operator=(
      const UntrustedAnnotatorPageHandlerImpl&) = delete;
  ~UntrustedAnnotatorPageHandlerImpl() override;

  // Called by ProjectorAppClient.
  void SetTool(const AnnotatorTool& tool);
  void Undo();
  void Redo();
  void Clear();

  // annotator::mojom::AnnotatorHandler:
  void OnUndoRedoAvailabilityChanged(bool undo_available,
                                     bool redo_available) override;
  void OnCanvasInitialized(bool success) override;

  content::WebUI* get_web_ui_for_test() { return web_ui_; }

 private:
  mojo::Remote<annotator::mojom::UntrustedAnnotatorPage> annotator_remote_;
  mojo::Receiver<annotator::mojom::UntrustedAnnotatorPageHandler>
      annotator_handler_receiver_;

  // The WebUI that owns the TrustedProjectorAnnotatorUI that owns this
  // instance.
  const raw_ptr<content::WebUI> web_ui_;
};

}  // namespace ash

#endif  // ASH_WEBUI_ANNOTATOR_UNTRUSTED_ANNOTATOR_PAGE_HANDLER_IMPL_H_
