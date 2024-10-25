// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_GRADUATION_GRADUATION_UI_HANDLER_H_
#define ASH_WEBUI_GRADUATION_GRADUATION_UI_HANDLER_H_

#include <memory>
#include <string>

#include "ash/webui/graduation/graduation_state_tracker.h"
#include "ash/webui/graduation/mojom/graduation_ui.mojom.h"
#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash::graduation {

class WebviewAuthHandler;

// WebUI handler for the Graduation App.
class GraduationUiHandler : public graduation_ui::mojom::GraduationUiHandler {
 public:
  // API that exposes methods for testing.
  class TestApi {
   public:
    explicit TestApi(GraduationUiHandler* handler);
    TestApi(const TestApi&) = delete;
    TestApi& operator=(const TestApi&) = delete;
    ~TestApi();

    WebviewAuthHandler* GetWebviewAuthHandler();

   private:
    const raw_ptr<GraduationUiHandler> handler_;
  };

  GraduationUiHandler(
      mojo::PendingReceiver<graduation_ui::mojom::GraduationUiHandler> receiver,
      std::unique_ptr<WebviewAuthHandler> auth_handler);

  GraduationUiHandler(const GraduationUiHandler&) = delete;
  GraduationUiHandler& operator=(const GraduationUiHandler&) = delete;
  ~GraduationUiHandler() override;

  // graduation_ui::mojom::GraduationUiHandler:
  void AuthenticateWebview(AuthenticateWebviewCallback callback) override;
  void GetProfileInfo(GetProfileInfoCallback callback) override;
  void OnScreenSwitched(graduation_ui::mojom::GraduationScreen screen) override;
  void OnTransferComplete() override;

 private:
  friend class GraduationUiHandlerTest;

  void OnAuthenticationFinished(AuthenticateWebviewCallback callback,
                                bool is_success);

  mojo::Receiver<graduation_ui::mojom::GraduationUiHandler> receiver_;

  // Helper for authenticating webview.
  std::unique_ptr<WebviewAuthHandler> auth_handler_;

  // Tracks the current state of the flow, used for metrics.
  GraduationStateTracker state_tracker_;
};

}  // namespace ash::graduation

#endif  // ASH_WEBUI_GRADUATION_GRADUATION_UI_HANDLER_H_
