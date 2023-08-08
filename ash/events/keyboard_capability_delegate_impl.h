// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_EVENTS_KEYBOARD_CAPABILITY_DELEGATE_IMPL_H_
#define ASH_EVENTS_KEYBOARD_CAPABILITY_DELEGATE_IMPL_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
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
  bool TopRowKeysAreFKeys() const override;
  void SetTopRowKeysAsFKeysEnabledForTesting(bool enabled) override;

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* prefs) override;

 private:
  // Initiate user preferences with given pref service.
  void InitUserPrefs(PrefService* prefs);

  // An observer to listen for changes to prefs::kSendFunctionKeys.
  std::unique_ptr<BooleanPrefMember> top_row_are_f_keys_pref_;
};

}  // namespace ash

#endif  // ASH_EVENTS_KEYBOARD_CAPABILITY_DELEGATE_IMPL_H_
