// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ANNOTATOR_UNTRUSTED_ANNOTATOR_UI_H_
#define ASH_WEBUI_ANNOTATOR_UNTRUSTED_ANNOTATOR_UI_H_

#include "ash/webui/annotator/mojom/untrusted_annotator.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/untrusted_web_ui_controller.h"

namespace content {
class WebUIDataSource;
}  // namespace content

namespace ash {

class UntrustedAnnotatorPageHandlerImpl;

// A delegate used during data source creation to expose some //chrome
// functionality to the data source
class UntrustedAnnotatorUIDelegate {
 public:
  virtual ~UntrustedAnnotatorUIDelegate() {}
  // Takes a WebUIDataSource, and populates its load-time data.
  virtual void PopulateLoadTimeData(content::WebUIDataSource* source) = 0;
};

// The webui for chrome-untrusted://projector-annotator.
class UntrustedAnnotatorUI
    : public ui::UntrustedWebUIController,
      public annotator::mojom::UntrustedAnnotatorPageHandlerFactory {
 public:
  // UntrustedAnnotatorUI does not store the passed in
  // `UntrustedAnnotatorUIDelegate`.
  UntrustedAnnotatorUI(
      content::WebUI* web_ui,
      UntrustedAnnotatorUIDelegate* delegate);
  UntrustedAnnotatorUI(const UntrustedAnnotatorUI&) = delete;
  UntrustedAnnotatorUI& operator=(
      const UntrustedAnnotatorUI&) = delete;
  ~UntrustedAnnotatorUI() override;

  void BindInterface(
      mojo::PendingReceiver<
          annotator::mojom::UntrustedAnnotatorPageHandlerFactory> factory);

 private:
  // annotator::mojom::UntrustedAnnotatorPageHandlerFactory:
  void Create(
      mojo::PendingReceiver<annotator::mojom::UntrustedAnnotatorPageHandler>
          annotator_handler,
      mojo::PendingRemote<annotator::mojom::UntrustedAnnotatorPage> annotator)
      override;

  mojo::Receiver<annotator::mojom::UntrustedAnnotatorPageHandlerFactory>
      receiver_{this};

  // Handler for requests coming from the web_ui.
  std::unique_ptr<UntrustedAnnotatorPageHandlerImpl> handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash

#endif  // ASH_WEBUI_ANNOTATOR_UNTRUSTED_ANNOTATOR_UI_H_
