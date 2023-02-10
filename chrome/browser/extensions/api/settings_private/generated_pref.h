// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_GENERATED_PREF_H_
#define CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_GENERATED_PREF_H_

#include <string>
#include "base/observer_list.h"
#include "chrome/browser/extensions/api/settings_private/prefs_util_enums.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/prefs/pref_service.h"

namespace base {
class Value;
}

namespace extensions {
namespace api {
namespace settings_private {
struct PrefObject;
}  // namespace settings_private
}  // namespace api

namespace settings_private {

// Base class for generated preference implementation.
// These are the "preferences" that exist in settings_private API only
// to simplify creating Settings UI for something not directly attached to
// user preference.
class GeneratedPref {
 public:
  class Observer {
   public:
    Observer();

    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;

    virtual ~Observer();

    // This method is called to notify observer that visible value
    // of the preference has changed.
    virtual void OnGeneratedPrefChanged(const std::string& pref_name) = 0;
  };

  GeneratedPref(const GeneratedPref&) = delete;
  GeneratedPref& operator=(const GeneratedPref&) = delete;

  virtual ~GeneratedPref();

  // Returns fully populated PrefObject.
  virtual api::settings_private::PrefObject GetPrefObject() const = 0;

  // Updates "preference" value.
  virtual SetPrefResult SetPref(const base::Value* value) = 0;

  // Modify observer list.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  GeneratedPref();

  // Call this when the pref value changes.
  void NotifyObservers(const std::string& pref_name);

  // Sets controlled_by for |pref_object| based on provided |pref| for a limited
  // subset of controlled_by sources relevant to generated pref use cases.
  static void ApplyControlledByFromPref(
      api::settings_private::PrefObject* pref_object,
      const PrefService::Preference* pref);

  // Sets controlled_by for |pref_object| base on provided |setting_source|
  // for a limited subset of controlled_by sources relevant for content
  // settings.
  static void ApplyControlledByFromContentSettingSource(
      api::settings_private::PrefObject* pref_object,
      content_settings::SettingSource setting_source);

  // Adds the provided |value| to the user selectable values of |pref_object|,
  // creating the base::Value vector if required.
  static void AddUserSelectableValue(
      extensions::api::settings_private::PrefObject* pref_object,
      int value);

 private:
  base::ObserverList<Observer>::Unchecked observers_;
};

}  // namespace settings_private
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_GENERATED_PREF_H_
