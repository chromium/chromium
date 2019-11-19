// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_INPUT_METHOD_MANAGER_INPUT_CONNECTION_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_ARC_INPUT_METHOD_MANAGER_INPUT_CONNECTION_IMPL_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/arc/input_method_manager/arc_input_method_manager_bridge.h"
#include "chrome/browser/chromeos/input_method/input_method_engine.h"
#include "components/arc/mojom/input_method_manager.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace arc {

// The implementation of mojom::InputConnection interface. It's generated for
// each text field and accepts text edit commands from the ARC container.
class InputConnectionImpl : public mojom::InputConnection {
 public:
  InputConnectionImpl(chromeos::InputMethodEngine* ime_engine,
                      ArcInputMethodManagerBridge* imm_bridge,
                      int input_context_id);
  ~InputConnectionImpl() override;

  // Binds this class to a passed interface pointer.
  void Bind(mojom::InputConnectionPtr* interface_ptr);
  // Sends current text input state to the ARC container.
  void UpdateTextInputState(bool is_input_state_update_requested);
  // Gets current text input state.
  mojom::TextInputStatePtr GetTextInputState(
      bool is_input_state_update_requested) const;

  // mojom::InputConnection overrides:
  void CommitText(const base::string16& text, int new_cursor_pos) override;
  void DeleteSurroundingText(int before, int after) override;
  void FinishComposingText() override;
  void SetComposingText(
      const base::string16& text,
      int new_cursor_pos,
      const base::Optional<gfx::Range>& new_selection_range) override;
  void RequestTextInputState(
      mojom::InputConnection::RequestTextInputStateCallback callback) override;
  void SetSelection(const gfx::Range& new_selection_range) override;
  void SendKeyEvent(mojom::KeyEventDataPtr data_ptr) override;
  void SetCompositionRange(const gfx::Range& new_composition_range) override;

 private:
  // Starts the timer to send new TextInputState.
  // This method should be called before doing any IME operation to catch state
  // update surely. Some implementations of TextInputClient are synchronous. If
  // starting timer is after API call, the timer won't be cancelled.
  void StartStateUpdateTimer();

  void SendControlKeyEvent(const base::string16& text);

  chromeos::InputMethodEngine* const ime_engine_;  // Not owned
  ArcInputMethodManagerBridge* const imm_bridge_;  // Not owned
  const int input_context_id_;

  mojo::Binding<mojom::InputConnection> binding_;

  base::OneShotTimer state_update_timer_;

  DISALLOW_COPY_AND_ASSIGN(InputConnectionImpl);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_INPUT_METHOD_MANAGER_INPUT_CONNECTION_IMPL_H_
