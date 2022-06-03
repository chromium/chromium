// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_DEMO_MODE_APP_UI_DEMO_MODE_PAGE_HANDLER_H_
#define ASH_WEBUI_DEMO_MODE_APP_UI_DEMO_MODE_PAGE_HANDLER_H_

#include "ash/webui/demo_mode_app_ui/mojom/demo_mode_app_ui.mojom.h"

#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/views/widget/widget.h"

namespace ash {

class DemoModePageHandler : public mojom::demo_mode::PageHandler {
 public:
  DemoModePageHandler(
      mojo::PendingReceiver<mojom::demo_mode::PageHandler> pending_receiver,
      views::Widget* widget);
  ~DemoModePageHandler() override;

  explicit DemoModePageHandler(const PageHandler&) = delete;
  DemoModePageHandler& operator=(const PageHandler&) = delete;

 private:
  // Switch between fullscreen and not-fullscreen
  void ToggleFullscreen() override;

  mojo::Receiver<mojom::demo_mode::PageHandler> receiver_;

  views::Widget* widget_;
};

}  // namespace ash

#endif  // ASH_WEBUI_DEMO_MODE_APP_UI_DEMO_MODE_PAGE_HANDLER_H_
