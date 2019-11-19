// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_SYNCER_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_SYNCER_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "components/prefs/pref_member.h"
#include "components/sync_preferences/pref_service_syncable_observer.h"
#include "ui/base/ime/chromeos/input_method_manager.h"

namespace sync_preferences {
class PrefServiceSyncable;
}  // namespace sync_preferences

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace chromeos {
namespace input_method {

// Helper class to handle syncing of language and input method preferences.
// Changes to local preferences are handed up to the sync server. But Chrome OS
// should not locally apply the corresponding preferences from the sync server,
// except once: when the user first logs into the device.
// Thus, the user's most recent changes to language and input method preferences
// will be brought down when signing in to a new device but not in future syncs.
class InputMethodSyncer : public sync_preferences::PrefServiceSyncableObserver {
 public:
  InputMethodSyncer(
      sync_preferences::PrefServiceSyncable* prefs,
      scoped_refptr<input_method::InputMethodManager::State> ime_state);
  ~InputMethodSyncer() override;

  // Registers the syncable input method prefs.
  static void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry);

  // Must be called after InputMethodSyncer is created.
  void Initialize();

 private:
  // Adds the input methods from the syncable prefs to the device-local prefs.
  // This should only be called once (after user's first sync) and only adds
  // to, not removes from, the user's input method prefs.
  void MergeSyncedPrefs();

  // For the given input method pref, adds unique values from |synced_pref| to
  // values in |pref|. The new values are converted from legacy engine IDs to
  // input method IDs if necessary.
  std::string AddSupportedInputMethodValues(
      const std::string& pref,
      const std::string& synced_pref,
      const char* pref_name);

  // Sets language::prefs::kPreferredLanguages and sets |merging_| to false.
  void FinishMerge(const std::string& languages);

  // Callback method for preference changes. Updates the syncable prefs using
  // the local pref values.
  void OnPreferenceChanged(const std::string& pref_name);

  // sync_preferences::PrefServiceSyncableObserver implementation.
  void OnIsSyncingChanged() override;

  StringPrefMember preferred_languages_;
  StringPrefMember preload_engines_;
  StringPrefMember enabled_imes_;
  // These are syncable variants which don't change the device settings. We can
  // set these to keep track of the user's most recent choices. That way, after
  // the initial sync, we can add the user's synced choices to the values that
  // have already been chosen at OOBE.
  StringPrefMember preferred_languages_syncable_;
  StringPrefMember preload_engines_syncable_;
  StringPrefMember enabled_imes_syncable_;

  sync_preferences::PrefServiceSyncable* prefs_;
  scoped_refptr<input_method::InputMethodManager::State> ime_state_;

  // Used to ignore PrefChanged events while InputMethodManager is merging.
  bool merging_;

  base::WeakPtrFactory<InputMethodSyncer> weak_factory_{this};
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_SYNCER_H_
