// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ECHE_APP_UI_ECHE_KEYBOARD_LAYOUT_HANDLER_H_
#define ASH_WEBUI_ECHE_APP_UI_ECHE_KEYBOARD_LAYOUT_HANDLER_H_

#include "ash/ime/ime_controller_impl.h"
#include "ash/webui/eche_app_ui/mojom/eche_app.mojom.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::eche_app {

// Implements the KeyboardLayoutHandler interface and observes ImeControllerImpl
// to relay keyboard layout/configuration information from ChromeOS to the Eche
// SWA. Additionally, provides the current keyboard layout to the SWA upon
// request.
class EcheKeyboardLayoutHandler : public mojom::KeyboardLayoutHandler,
                                  public ImeController::Observer {
 public:
  EcheKeyboardLayoutHandler();
  ~EcheKeyboardLayoutHandler() override;

  // mojom::KeyboardLayoutHandler:
  void RequestCurrentKeyboardLayout() override;
  void SetKeyboardLayoutObserver(
      mojo::PendingRemote<mojom::KeyboardLayoutObserver> observer) override;

  // ImeController::Observer:
  void OnCapsLockChanged(bool enabled) override;
  void OnKeyboardLayoutNameChanged(const std::string& layout_name) override;

  void Bind(mojo::PendingReceiver<mojom::KeyboardLayoutHandler> receiver);

 private:
  friend class EcheKeyboardLayoutHandlerTest;

  mojo::Receiver<mojom::KeyboardLayoutHandler> keyboard_layout_handler_{this};
  mojo::Remote<mojom::KeyboardLayoutObserver> remote_observer_;
};

}  // namespace ash::eche_app

#endif  // ASH_WEBUI_ECHE_APP_UI_ECHE_KEYBOARD_LAYOUT_HANDLER_H_
