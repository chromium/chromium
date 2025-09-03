// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_BOCA_RECEIVER_APP_UI_BOCA_RECEIVER_UNTRUSTED_UI_H_
#define ASH_WEBUI_BOCA_RECEIVER_APP_UI_BOCA_RECEIVER_UNTRUSTED_UI_H_

#include "ash/webui/boca_receiver_app_ui/mojom/boca_receiver.mojom.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/untrusted_web_ui_controller.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace ash {

namespace boca_receiver {
class BocaReceiverUntrustedPageHandler;
}  // namespace boca_receiver

class BocaReceiverUntrustedUI;

class BocaReceiverUntrustedUIConfig
    : public content::DefaultWebUIConfig<BocaReceiverUntrustedUI> {
 public:
  BocaReceiverUntrustedUIConfig();

  BocaReceiverUntrustedUIConfig(const BocaReceiverUntrustedUIConfig&) = delete;
  BocaReceiverUntrustedUIConfig& operator=(
      const BocaReceiverUntrustedUIConfig&) = delete;

  ~BocaReceiverUntrustedUIConfig() override;

  // content::DefaultWebUIConfig
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

class BocaReceiverUntrustedUI
    : public ui::UntrustedWebUIController,
      public boca_receiver::mojom::UntrustedPageHandlerFactory {
 public:
  explicit BocaReceiverUntrustedUI(content::WebUI* web_ui);

  BocaReceiverUntrustedUI(const BocaReceiverUntrustedUI&) = delete;
  BocaReceiverUntrustedUI& operator=(const BocaReceiverUntrustedUI&) = delete;

  ~BocaReceiverUntrustedUI() override;

  void BindInterface(
      mojo::PendingReceiver<boca_receiver::mojom::UntrustedPageHandlerFactory>
          receiver);

 private:
  // boca_receiver::mojom::UntrustedPageHandlerFactory:
  void CreateUntrustedPageHandler(
      mojo::PendingRemote<boca_receiver::mojom::UntrustedPage> page) override;

  mojo::Receiver<boca_receiver::mojom::UntrustedPageHandlerFactory>
      page_factory_receiver_{this};

  std::unique_ptr<boca_receiver::BocaReceiverUntrustedPageHandler>
      page_handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash

#endif  // ASH_WEBUI_BOCA_RECEIVER_APP_UI_BOCA_RECEIVER_UNTRUSTED_UI_H_
