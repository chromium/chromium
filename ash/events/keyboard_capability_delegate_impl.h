// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_EVENTS_KEYBOARD_CAPABILITY_DELEGATE_IMPL_H_
#define ASH_EVENTS_KEYBOARD_CAPABILITY_DELEGATE_IMPL_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/observer_list.h"
#include "components/prefs/pref_member.h"
#include "ui/events/ash/keyboard_capability.h"

class PrefService;

namespace ash {

// KeyboardCapabilityDelegateImpl implements KeyboardCapability Delegate. It
// provides various keyboard capability information such as if top row keys are
// treated as function keys.
class ASH_EXPORT KeyboardCapabilityDelegateImpl
    : public ui::KeyboardCapability::Delegate,
      public SessionObserver {
 public:
  KeyboardCapabilityDelegateImpl();
  KeyboardCapabilityDelegateImpl(const KeyboardCapabilityDelegateImpl&) =
      delete;
  KeyboardCapabilityDelegateImpl& operator=(
      const KeyboardCapabilityDelegateImpl&) = delete;
  ~KeyboardCapabilityDelegateImpl() override;

  // ui::KeyboardCapability::Delegate:
  void AddObserver(ui::KeyboardCapability::Observer* observer) override;
  void RemoveObserver(ui::KeyboardCapability::Observer* observer) override;
  bool TopRowKeysAreFKeys() const override;
  void SetTopRowKeysAsFKeysEnabledForTesting(bool enabled) override;
  bool IsPrivacyScreenSupported() const override;
  void SetPrivacyScreenSupportedForTesting(bool is_supported) override;

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* prefs) override;

 private:
  // Initiate user preferences with given pref service.
  void InitUserPrefs(PrefService* prefs);

  void NotifyTopRowKeysAreFKeysChanged();

  // An observer to listen for changes to prefs::kSendFunctionKeys.
  std::unique_ptr<BooleanPrefMember> top_row_are_f_keys_pref_;

  // Used only for testing.
  absl::optional<bool> is_privacy_screen_supported_for_testing_;

  // A list of KeyboardCapability Observers.
  base::ObserverList<ui::KeyboardCapability::Observer>::Unchecked observers_;
};

}  // namespace ash

#endif  // ASH_EVENTS_KEYBOARD_CAPABILITY_DELEGATE_IMPL_H_
