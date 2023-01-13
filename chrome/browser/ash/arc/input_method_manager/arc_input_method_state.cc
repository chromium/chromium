// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_method_manager/arc_input_method_state.h"

#include "ash/components/arc/mojom/input_method_manager.mojom.h"
#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "ash/public/cpp/tablet_mode.h"
#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "components/prefs/pref_service.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "ui/base/ime/ash/extension_ime_util.h"

namespace arc {

using InputMethodDescriptor = ::ash::input_method::InputMethodDescriptor;
using InputMethodDescriptors = ::ash::input_method::InputMethodDescriptors;

ArcInputMethodState::ArcInputMethodState(const Delegate* const delegate)
    : delegate_(delegate) {}
ArcInputMethodState::~ArcInputMethodState() = default;

void ArcInputMethodState::InitializeWithImeInfo(
    const std::string& proxy_ime_extension_id,
    const std::vector<mojom::ImeInfoPtr>& ime_info_array) {
  installed_imes_.clear();
  for (const auto& info : ime_info_array) {
    installed_imes_.push_back({info->ime_id, info->enabled,
                               info->is_allowed_in_clamshell_mode,
                               delegate_->BuildInputMethodDescriptor(info)});
  }
}

void ArcInputMethodState::DisableInputMethod(const std::string& ime_id) {
  SetInputMethodEnabled(ime_id, false);
}

InputMethodDescriptors ArcInputMethodState::GetAvailableInputMethods() const {
  const bool all_allowed = delegate_->ShouldArcIMEAllowed();

  InputMethodDescriptors result;
  for (const auto& entry : installed_imes_) {
    if (all_allowed || entry.always_allowed_)
      result.push_back(entry.descriptor_);
  }
  return result;
}

InputMethodDescriptors ArcInputMethodState::GetEnabledInputMethods() const {
  const bool all_allowed = delegate_->ShouldArcIMEAllowed();

  InputMethodDescriptors result;
  for (const auto& entry : installed_imes_) {
    if (entry.enabled_ && (all_allowed || entry.always_allowed_))
      result.push_back(entry.descriptor_);
  }
  return result;
}

void ArcInputMethodState::SetInputMethodEnabled(const std::string& ime_id,
                                                bool enabled) {
  auto it =
      base::ranges::find(installed_imes_, ime_id, &InputMethodEntry::ime_id_);
  if (it == installed_imes_.end()) {
    // Ignore the request to enable/disable not-installed IME.
    return;
  }

  it->enabled_ = enabled;
}

}  // namespace arc
