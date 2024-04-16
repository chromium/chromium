// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_IME_CONTROLLER_CLIENT_H_
#define ASH_PUBLIC_CPP_IME_CONTROLLER_CLIENT_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "base/functional/callback.h"
#include "ui/base/ime/ash/ime_keyset.h"

namespace ash {

// Interface for ash to send input method requests to its client (e.g. Chrome).
class ASH_PUBLIC_EXPORT ImeControllerClient {
 public:
  // Switches to the next input method. Does nothing if only one input method
  // is installed.
  virtual void SwitchToNextIme() = 0;

  // Switches to the last used input method. Does nothing if only one input
  // method is installed.
  virtual void SwitchToLastUsedIme() = 0;

  // Switches to an input method by |id|. Does nothing if the input method is
  // not installed. The ID is usually the output of a call like
  // extension_ime_util::GetInputMethodIDByEngineID("xkb:jp::jpn"),
  // see that function for details. Shows a bubble with the input method short
  // name when |show_message| is true.
  virtual void SwitchImeById(const std::string& id, bool show_message) = 0;

  // Activates an input method menu item. The |key| must be a value from the
  // ImeMenuItems provided via RefreshIme. Does nothing if the |key| is invalid.
  virtual void ActivateImeMenuItem(const std::string& key) = 0;

  // When the caps lock state change originates from the tray (i.e. clicking the
  // caps lock toggle from the settings menu from the caps lock icon), from an
  // accelerator (e.g. pressing Alt + Search), or from the debug UI (i.e.
  // toggling the caps lock button), propagate the change to the client without
  // sending a change notification back.
  // TODO(crbug.com/40537240): Ideally this interaction should only be to
  // disable the caps lock.
  virtual void SetCapsLockEnabled(bool enabled) = 0;

  // Overrides the keyboard keyset (emoji, handwriting or voice). If keyset is
  // 'kNone', we switch to the default keyset. Because this is asynchronous,
  // any code that needs the keyset to be updated first must use the callback.
  using OverrideKeyboardKeysetCallback = base::OnceCallback<void()>;
  virtual void OverrideKeyboardKeyset(
      input_method::ImeKeyset keyset,
      OverrideKeyboardKeysetCallback callback) = 0;

  // Show the current mode.
  virtual void ShowModeIndicator() = 0;
};
}  // namespace ash

#endif  // ASH_PUBLIC_CPP_IME_CONTROLLER_CLIENT_H_
