// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CONTENT_SCANNING_SCANNING_UI_H_
#define ASH_CONTENT_SCANNING_SCANNING_UI_H_

#include <memory>

#include "ash/content/scanning/mojom/scanning.mojom-forward.h"
#include "ash/content/scanning/scanning_handler.h"
#include "base/callback.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace content {
class WebUI;
}  // namespace content

namespace ash {

class ScanningAppDelegate;

// The WebUI for chrome://scanning.
class ScanningUI : public ui::MojoWebUIController {
 public:
  using BindScanServiceCallback = base::RepeatingCallback<void(
      mojo::PendingReceiver<scanning::mojom::ScanService>)>;

  // |callback| should bind the pending receiver to an implementation of
  // ash::scanning::mojom::ScanService.
  ScanningUI(content::WebUI* web_ui,
             BindScanServiceCallback callback,
             std::unique_ptr<ScanningAppDelegate> scanning_app_delegate);
  ~ScanningUI() override;

  ScanningUI(const ScanningUI&) = delete;
  ScanningUI& operator=(const ScanningUI&) = delete;

  // Instantiates the implementor of the ash::scanning::mojom::ScanService
  // Mojo interface by passing the pending receiver that will be internally
  // bound.
  void BindInterface(
      mojo::PendingReceiver<scanning::mojom::ScanService> pending_receiver);

 private:
  const BindScanServiceCallback bind_pending_receiver_callback_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash

#endif  // ASH_CONTENT_SCANNING_SCANNING_UI_H_
