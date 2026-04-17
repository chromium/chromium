// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_method_manager/arc_input_method_state.h"

#include <algorithm>

#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "ash/public/cpp/tablet_mode.h"
#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "chromeos/ash/experiences/arc/mojom/input_method_manager.mojom.h"
#include "components/prefs/pref_service.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "ui/base/ime/ash/extension_ime_util.h"
#include "url/gurl.h"

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
  base::flat_set<std::string> seen_ime_ids;
  for (const auto& info : ime_info_array) {
    if (info->ime_id.empty()) {
      LOG(WARNING) << "Rejecting ImeInfo with empty ime_id.";
      continue;
    }

    // Prevent delimiter injection in prefs (comma is used as a delimiter).
    if (info->ime_id.find(',') != std::string::npos) {
      LOG(WARNING) << "Rejecting ImeInfo with invalid ime_id (contains comma).";
      continue;
    }

    if (!seen_ime_ids.insert(info->ime_id).second) {
      LOG(WARNING) << "Rejecting duplicate ImeInfo for ime_id: "
                   << info->ime_id;
      continue;
    }

    // Only allow valid 'intent' scheme or empty.
    GURL settings_url(info->settings_url);
    if (!settings_url.is_empty() &&
        (!settings_url.is_valid() || !settings_url.SchemeIs("intent"))) {
      LOG(WARNING)
          << "Ignoring invalid or non-intent settings URL for ARC IME: "
          << info->settings_url;
      info->settings_url = "";
    }

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
      std::ranges::find(installed_imes_, ime_id, &InputMethodEntry::ime_id_);
  if (it == installed_imes_.end()) {
    // Ignore the request to enable/disable not-installed IME.
    return;
  }

  it->enabled_ = enabled;
}

}  // namespace arc
