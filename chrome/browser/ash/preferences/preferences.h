// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PREFERENCES_PREFERENCES_H_
#define CHROME_BROWSER_ASH_PREFERENCES_PREFERENCES_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine_client.h"
#include "chromeos/ash/components/language_preferences/language_preferences.h"
#include "chromeos/crosapi/mojom/cros_display_config.mojom.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_member.h"
#include "components/sync_preferences/pref_service_syncable_observer.h"
#include "components/user_manager/user_manager.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/base/ime/ash/input_method_manager.h"

class ContentTracingManager;
class PrefRegistrySimple;

namespace chromeos {
class User;
}

namespace sync_preferences {
class PrefServiceSyncable;
}  // namespace sync_preferences

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace ash {

namespace input_method {
class InputMethodSyncer;
}

// The Preferences class handles Chrome OS preferences. When the class
// is first initialized, it will initialize the OS settings to what's stored in
// the preferences. These include touchpad settings, etc.
// When the preferences change, we change the settings to reflect the new value.
class Preferences : public sync_preferences::PrefServiceSyncableObserver,
                    public user_manager::UserManager::UserSessionStateObserver,
                    public UpdateEngineClient::Observer {
 public:
  Preferences();
  explicit Preferences(
      input_method::InputMethodManager* input_method_manager);  // for testing

  Preferences(const Preferences&) = delete;
  Preferences& operator=(const Preferences&) = delete;

  ~Preferences() override;

  // These method will register the prefs associated with Chrome OS settings.
  static void RegisterPrefs(PrefRegistrySimple* registry);
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // This method will initialize Chrome OS settings to values in user prefs.
  // |user| is the user owning this preferences.
  void Init(Profile* profile, const user_manager::User* user);

  void InitPrefsForTesting(sync_preferences::PrefServiceSyncable* prefs);
  void InitUserPrefsForTesting(
      sync_preferences::PrefServiceSyncable* prefs,
      const user_manager::User* user,
      scoped_refptr<input_method::InputMethodManager::State> ime_state);
  void SetInputMethodListForTesting();

 private:
  enum ApplyReason {
    REASON_INITIALIZATION,
    REASON_ACTIVE_USER_CHANGED,
    REASON_PREF_CHANGED
  };

  // Initializes all member prefs.
  void InitUserPrefs(sync_preferences::PrefServiceSyncable* prefs);

  // Callback method for preference changes.
  void OnPreferenceChanged(const std::string& pref_name);

  // Add a sample to the appropriate UMA histogram for a boolean preference.
  void ReportBooleanPrefApplication(ApplyReason reason,
                                    const std::string& changed_histogram_name,
                                    const std::string& started_histogram_name,
                                    bool sample);

  // Add a sample to the appropriate UMA histogram for a sensitivity preference.
  void ReportSensitivityPrefApplication(
      ApplyReason reason,
      const std::string& changed_histogram_name,
      const std::string& started_histogram_name,
      int sensitivity_int);

  // Add a sample to the appropriate UMA histogram for a time/rate preference.
  void ReportTimePrefApplication(ApplyReason reason,
                                 const std::string& changed_histogram_name,
                                 const std::string& started_histogram_name,
                                 base::TimeDelta duration);

  // This will set the OS settings when the preference changed or the user
  // owning these preferences became active. Also this method is called on
  // initialization. The reason of the call is stored as the |reason| parameter.
  // |pref_name| is the name of the changed preference if the |reason| is
  // |REASON_PREF_CHANGED|, otherwise it is empty.
  void ApplyPreferences(ApplyReason reason,
                        const std::string& pref_name);

  // A variant of SetLanguageConfigStringList. You can pass comma-separated
  // values. Examples of |value|: "", "Control+space,Hiragana"
  void SetLanguageConfigStringListAsCSV(const char* section,
                                        const char* name,
                                        const std::string& value);

  // Restores the user's preferred input method / keyboard layout on signing in.
  void SetInputMethodList();

  // Updates the initial key repeat delay and key repeat interval following
  // current prefs values. We set the delay and interval at once since an
  // underlying XKB API requires it.
  void UpdateAutoRepeatRate();

  // Force natural scroll to on if --enable-natural-scroll-default is specified
  // on the cmd line.
  void ForceNaturalScrollDefault();

  // sync_preferences::PrefServiceSyncableObserver implementation.
  void OnIsSyncingChanged() override;

  // Overriden form user_manager::UserManager::UserSessionStateObserver.
  void ActiveUserChanged(user_manager::User* active_user) override;

  // UpdateEngineClient::Observer implementation.
  void UpdateStatusChanged(const update_engine::StatusResult& status) override;
  void OnIsConsumerAutoUpdateEnabled(std::optional<bool> enabled);

  raw_ptr<sync_preferences::PrefServiceSyncable> prefs_;

  raw_ptr<input_method::InputMethodManager> input_method_manager_;
  std::unique_ptr<ContentTracingManager> tracing_manager_;

  BooleanPrefMember performance_tracing_enabled_;
  BooleanPrefMember tap_to_click_enabled_;
  BooleanPrefMember three_finger_click_enabled_;
  BooleanPrefMember unified_desktop_enabled_by_default_;
  BooleanPrefMember natural_scroll_;
  BooleanPrefMember vert_edge_scroll_enabled_;
  IntegerPrefMember speed_factor_;
  IntegerPrefMember mouse_sensitivity_;
  IntegerPrefMember mouse_scroll_sensitivity_;
  IntegerPrefMember pointing_stick_sensitivity_;
  IntegerPrefMember touchpad_sensitivity_;
  IntegerPrefMember touchpad_scroll_sensitivity_;
  BooleanPrefMember primary_mouse_button_right_;
  BooleanPrefMember primary_pointing_stick_button_right_;
  BooleanPrefMember mouse_reverse_scroll_;
  BooleanPrefMember mouse_acceleration_;
  BooleanPrefMember mouse_scroll_acceleration_;
  BooleanPrefMember pointing_stick_acceleration_;
  BooleanPrefMember touchpad_acceleration_;
  BooleanPrefMember touchpad_scroll_acceleration_;
  BooleanPrefMember touchpad_haptic_feedback_;
  IntegerPrefMember touchpad_haptic_click_sensitivity_;
  FilePathPrefMember download_default_directory_;

  StringListPrefMember allowed_languages_;
  StringPrefMember preferred_languages_;

  // Input method preferences.
  StringPrefMember preload_engines_;
  StringPrefMember current_input_method_;
  StringPrefMember previous_input_method_;

  StringListPrefMember allowed_input_methods_;
  StringPrefMember enabled_imes_;
  BooleanPrefMember ime_menu_activated_;

  BooleanPrefMember long_press_diacritics_enabled_;
  BooleanPrefMember xkb_auto_repeat_enabled_;
  IntegerPrefMember xkb_auto_repeat_delay_pref_;
  IntegerPrefMember xkb_auto_repeat_interval_pref_;

  BooleanPrefMember pci_data_access_enabled_pref_;

  BooleanPrefMember consumer_auto_update_toggle_pref_;

  PrefChangeRegistrar pref_change_registrar_;

  // User owning these preferences.
  raw_ptr<const user_manager::User> user_;

  // Whether user is a primary user.
  bool user_is_primary_;

  // Input Methods state for this user.
  scoped_refptr<input_method::InputMethodManager::State> ime_state_;

  std::unique_ptr<input_method::InputMethodSyncer> input_method_syncer_;

  mojo::Remote<crosapi::mojom::CrosDisplayConfigController>
      cros_display_config_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<Preferences> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PREFERENCES_PREFERENCES_H_
