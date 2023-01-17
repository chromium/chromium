// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_INPUT_METHOD_TEST_INTERFACE_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_INPUT_METHOD_TEST_INTERFACE_ASH_H_

#include <string>

#include "base/callback_list.h"
#include "base/scoped_observation.h"
#include "chromeos/crosapi/mojom/test_controller.mojom.h"
#include "ui/base/ime/ash/input_method_ash.h"
#include "ui/base/ime/input_method_observer.h"

namespace crosapi {

class InputMethodTestInterfaceAsh : public mojom::InputMethodTestInterface,
                                    public ui::InputMethodObserver {
 public:
  InputMethodTestInterfaceAsh();
  ~InputMethodTestInterfaceAsh() override;
  InputMethodTestInterfaceAsh(const InputMethodTestInterfaceAsh&) = delete;
  InputMethodTestInterfaceAsh& operator=(const InputMethodTestInterfaceAsh&) =
      delete;

  // mojom::InputMethodTestInterface:
  void WaitForFocus(WaitForFocusCallback callback) override;
  void CommitText(const std::string& text,
                  CommitTextCallback callback) override;
  void SetComposition(const std::string& text,
                      uint32_t index,
                      SetCompositionCallback callback) override;
  void SendKeyEvent(mojom::KeyEventPtr event,
                    SendKeyEventCallback callback) override;

  // ui::InputMethodObserver:
  void OnFocus() override {}
  void OnBlur() override {}
  void OnCaretBoundsChanged(const ui::TextInputClient* client) override {}
  void OnTextInputStateChanged(const ui::TextInputClient* client) override;
  void OnInputMethodDestroyed(const ui::InputMethod* input_method) override {}

 private:
  ash::InputMethodAsh* input_method_;
  base::ScopedObservation<ui::InputMethod, ui::InputMethodObserver>
      input_method_observation_{this};
  base::OnceClosureList focus_callbacks_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_INPUT_METHOD_TEST_INTERFACE_ASH_H_
