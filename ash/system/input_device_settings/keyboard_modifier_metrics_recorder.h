// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_KEYBOARD_MODIFIER_METRICS_RECORDER_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_KEYBOARD_MODIFIER_METRICS_RECORDER_H_

#include <array>
#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/public/mojom/input_device_settings.mojom-shared.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_member.h"
#include "ui/chromeos/events/pref_names.h"

class PrefService;
class PrefRegistrySimple;

namespace ash {

// Records metrics for whenever keyboard modifier settings change and when a
// user session is first initialized.
class ASH_EXPORT KeyboardModifierMetricsRecorder : public SessionObserver {
 public:
  // Do not change ordering of this list as the ordering is used to compute
  // modifier hash in `RecordModifierRemappingHash()`.
  static constexpr struct {
    const char* key_name;
    const char* pref_name;
    mojom::ModifierKey default_modifier_key;
  } kKeyboardModifierPrefs[] = {
      {"Alt", ::prefs::kLanguageRemapAltKeyTo, mojom::ModifierKey::kAlt},
      {"Control", ::prefs::kLanguageRemapControlKeyTo,
       mojom::ModifierKey::kControl},
      {"Escape", ::prefs::kLanguageRemapEscapeKeyTo,
       mojom::ModifierKey::kEscape},
      {"Backspace", ::prefs::kLanguageRemapBackspaceKeyTo,
       mojom::ModifierKey::kBackspace},
      {"Assistant", ::prefs::kLanguageRemapAssistantKeyTo,
       mojom::ModifierKey::kAssistant},
      {"CapsLock", ::prefs::kLanguageRemapCapsLockKeyTo,
       mojom::ModifierKey::kCapsLock},
      {"ExternalMeta", ::prefs::kLanguageRemapExternalMetaKeyTo,
       mojom::ModifierKey::kMeta},
      {"Search", ::prefs::kLanguageRemapSearchKeyTo, mojom::ModifierKey::kMeta},
      {"ExternalCommand", ::prefs::kLanguageRemapExternalCommandKeyTo,
       mojom::ModifierKey::kControl},
  };

  KeyboardModifierMetricsRecorder();
  KeyboardModifierMetricsRecorder(const KeyboardModifierMetricsRecorder&) =
      delete;
  KeyboardModifierMetricsRecorder& operator=(
      const KeyboardModifierMetricsRecorder&) = delete;
  ~KeyboardModifierMetricsRecorder() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry, bool for_test);

  void RecordModifierRemappingHash();

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

 private:
  void OnModifierRemappingChanged(size_t index, const std::string& pref_name);
  void ResetPrefMembers();

  void RecordModifierRemappingChanged(size_t index,
                                      mojom::ModifierKey modifier_key);
  void RecordModifierRemappingInit(size_t index,
                                   mojom::ModifierKey modifier_key);

  // TODO(dpad): Remove pref members once transitioned to per device settings.
  std::array<std::unique_ptr<IntegerPrefMember>,
             std::size(kKeyboardModifierPrefs)>
      pref_members_;

  base::flat_set<AccountId> recorded_accounts_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_KEYBOARD_MODIFIER_METRICS_RECORDER_H_
