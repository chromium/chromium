// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PROJECTOR_APP_TRUSTED_PROJECTOR_ANNOTATOR_UI_H_
#define ASH_WEBUI_PROJECTOR_APP_TRUSTED_PROJECTOR_ANNOTATOR_UI_H_

#include "ash/webui/projector_app/mojom/annotator.mojom.h"
#include "content/public/browser/web_ui_controller.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_bubble_web_ui_controller.h"

class GURL;
class PrefService;

namespace ash {

class AnnotatorPageHandlerImpl;

// The implementation for the Projector annotator for screen recording
// annotations.
class TrustedProjectorAnnotatorUI
    : public ui::MojoBubbleWebUIController,
      annotator::mojom::AnnotatorPageHandlerFactory {
 public:
  TrustedProjectorAnnotatorUI(content::WebUI* web_ui,
                              const GURL& url,
                              PrefService* pref_service);
  ~TrustedProjectorAnnotatorUI() override;
  TrustedProjectorAnnotatorUI(const TrustedProjectorAnnotatorUI&) = delete;
  TrustedProjectorAnnotatorUI& operator=(const TrustedProjectorAnnotatorUI&) =
      delete;

  void BindInterface(
      mojo::PendingReceiver<annotator::mojom::AnnotatorPageHandlerFactory>
          factory);

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();

  // annotator::mojom::AnnotatorPageHandlerFactory:
  void Create(
      mojo::PendingReceiver<annotator::mojom::AnnotatorPageHandler>
          annotator_handler,
      mojo::PendingRemote<annotator::mojom::AnnotatorPage> annotator) override;

  mojo::Receiver<annotator::mojom::AnnotatorPageHandlerFactory> receiver_{this};

  // Handler for requests coming from the web_ui.
  std::unique_ptr<AnnotatorPageHandlerImpl> handler_;
};

}  // namespace ash

#endif  // ASH_WEBUI_PROJECTOR_APP_TRUSTED_PROJECTOR_ANNOTATOR_UI_H_
