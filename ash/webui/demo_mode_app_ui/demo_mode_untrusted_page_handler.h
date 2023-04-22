// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_DEMO_MODE_APP_UI_DEMO_MODE_UNTRUSTED_PAGE_HANDLER_H_
#define ASH_WEBUI_DEMO_MODE_APP_UI_DEMO_MODE_UNTRUSTED_PAGE_HANDLER_H_

#include "ash/webui/demo_mode_app_ui/mojom/demo_mode_app_untrusted_ui.mojom.h"
#include "base/memory/raw_ptr.h"

#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/views/widget/widget.h"

namespace ash {

class DemoModeUntrustedPageHandler
    : public mojom::demo_mode::UntrustedPageHandler {
 public:
  DemoModeUntrustedPageHandler(
      mojo::PendingReceiver<mojom::demo_mode::UntrustedPageHandler>
          pending_receiver,
      views::Widget* widget);
  ~DemoModeUntrustedPageHandler() override;

  explicit DemoModeUntrustedPageHandler(const UntrustedPageHandler&) = delete;
  DemoModeUntrustedPageHandler& operator=(const UntrustedPageHandler&) = delete;

 private:
  // Switch between fullscreen and not-fullscreen
  void ToggleFullscreen() override;

  mojo::Receiver<mojom::demo_mode::UntrustedPageHandler> receiver_;

  raw_ptr<views::Widget, ExperimentalAsh> widget_;
};

}  // namespace ash

#endif  // ASH_WEBUI_DEMO_MODE_APP_UI_DEMO_MODE_UNTRUSTED_PAGE_HANDLER_H_
