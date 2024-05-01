// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_DEMO_MODE_APP_UI_DEMO_MODE_UNTRUSTED_PAGE_HANDLER_H_
#define ASH_WEBUI_DEMO_MODE_APP_UI_DEMO_MODE_UNTRUSTED_PAGE_HANDLER_H_

#include "ash/webui/demo_mode_app_ui/demo_mode_app_delegate.h"
#include "ash/webui/demo_mode_app_ui/demo_mode_app_untrusted_ui.h"
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
      views::Widget* widget,
      DemoModeAppUntrustedUI* demo_mode_app_untrusted_ui);
  ~DemoModeUntrustedPageHandler() override;

  explicit DemoModeUntrustedPageHandler(const UntrustedPageHandler&) = delete;
  DemoModeUntrustedPageHandler& operator=(const UntrustedPageHandler&) = delete;

 private:
  // Switch between fullscreen and non-fullscreen (windowed mode).
  void ToggleFullscreen() override;

  // Launch an app by App Service app_id.
  void LaunchApp(const std::string& app_id) override;

  mojo::Receiver<mojom::demo_mode::UntrustedPageHandler> receiver_;

  raw_ptr<views::Widget> widget_;

  raw_ptr<DemoModeAppUntrustedUI> demo_mode_app_untrusted_ui_;
};

}  // namespace ash

#endif  // ASH_WEBUI_DEMO_MODE_APP_UI_DEMO_MODE_UNTRUSTED_PAGE_HANDLER_H_
