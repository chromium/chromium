// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PROJECTOR_APP_UNTRUSTED_PROJECTOR_UI_H_
#define ASH_WEBUI_PROJECTOR_APP_UNTRUSTED_PROJECTOR_UI_H_

#include <memory>

#include "ash/webui/projector_app/mojom/untrusted_projector.mojom.h"
#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"
#include "ui/webui/untrusted_web_ui_controller.h"

namespace content {
class WebUIDataSource;
}  // namespace content

namespace ui {
class ColorChangeHandler;
}  // namespace ui

class PrefService;

namespace ash {

class UntrustedProjectorPageHandlerImpl;

// A delegate used during data source creation to expose some //chrome
// functionality to the data source
class UntrustedProjectorUIDelegate {
 public:
  // Takes a WebUIDataSource, and populates its load-time data.
  virtual void PopulateLoadTimeData(content::WebUIDataSource* source) = 0;
};

// The webui for chrome-untrusted://projector.
class UntrustedProjectorUI
    : public ui::UntrustedWebUIController,
      public projector::mojom::UntrustedProjectorPageHandlerFactory {
 public:
  UntrustedProjectorUI(content::WebUI* web_ui,
                       UntrustedProjectorUIDelegate* delegate,
                       PrefService* pref_service);
  UntrustedProjectorUI(const UntrustedProjectorUI&) = delete;
  UntrustedProjectorUI& operator=(const UntrustedProjectorUI&) = delete;
  ~UntrustedProjectorUI() override;

  void BindInterface(
      mojo::PendingReceiver<
          projector::mojom::UntrustedProjectorPageHandlerFactory> factory);

  // Binds a PageHandler to ProjectorUntrustedUI. This handler grabs a reference
  // to the page and pushes a colorChangeEvent to the untrusted JS running there
  // when the OS color scheme has changed.
  void BindInterface(
      mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
          receiver);

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();

  // projector::mojom::UntrustedProjectorPageHandlerFactory:
  void Create(
      mojo::PendingReceiver<projector::mojom::UntrustedProjectorPageHandler>
          projector_handler,
      mojo::PendingRemote<projector::mojom::UntrustedProjectorPage> projector)
      override;

  mojo::Receiver<projector::mojom::UntrustedProjectorPageHandlerFactory>
      receiver_{this};
  std::unique_ptr<UntrustedProjectorPageHandlerImpl> page_handler_;
  const raw_ptr<PrefService> pref_service_;

  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;
};

}  // namespace ash

#endif  // ASH_WEBUI_PROJECTOR_APP_UNTRUSTED_PROJECTOR_UI_H_
