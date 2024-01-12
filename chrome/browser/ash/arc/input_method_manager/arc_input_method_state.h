// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_METHOD_MANAGER_ARC_INPUT_METHOD_STATE_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_METHOD_MANAGER_ARC_INPUT_METHOD_STATE_H_

#include <set>
#include <string>
#include <vector>

#include "ash/components/arc/mojom/input_method_manager.mojom-forward.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/ime/ash/input_method_descriptor.h"

namespace arc {

// Model to state ARC IME's state (installed/enabled/allowed).
class ArcInputMethodState {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual bool ShouldArcIMEAllowed() const = 0;
    virtual ash::input_method::InputMethodDescriptor BuildInputMethodDescriptor(
        const mojom::ImeInfoPtr& info) const = 0;
  };

  explicit ArcInputMethodState(const Delegate* const delegate);
  ~ArcInputMethodState();

  ArcInputMethodState(const ArcInputMethodState& state) = delete;
  ArcInputMethodState& operator=(const ArcInputMethodState& state) = delete;

  // State updating methods:
  void InitializeWithImeInfo(
      const std::string& proxy_ime_extension_id,
      const std::vector<mojom::ImeInfoPtr>& ime_info_array);
  void DisableInputMethod(const std::string& ime_id);

  // Return the InputMethodDescriptors which are installed and allowed.
  ash::input_method::InputMethodDescriptors GetAvailableInputMethods() const;
  // Return the InputMethodDescriptors which are enabled and allowed.
  ash::input_method::InputMethodDescriptors GetEnabledInputMethods() const;

 private:
  class InputMethodEntry {
   public:
    std::string ime_id_;
    bool enabled_{false};
    bool always_allowed_{false};
    ash::input_method::InputMethodDescriptor descriptor_;
  };

  void SetInputMethodEnabled(const std::string& ime_id, bool enabled);

  const raw_ptr<const Delegate> delegate_;

  std::vector<InputMethodEntry> installed_imes_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_METHOD_MANAGER_ARC_INPUT_METHOD_STATE_H_
