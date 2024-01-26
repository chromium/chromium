// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_GENERATED_PREFS_H_
#define CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_GENERATED_PREFS_H_

#include <memory>
#include <optional>
#include <string>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/api/settings_private/generated_pref.h"
#include "chrome/browser/extensions/api/settings_private/prefs_util_enums.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace base {
class Value;
}

namespace extensions {

namespace api::settings_private {
struct PrefObject;
}  // namespace api::settings_private

namespace settings_private {

// This is a "store" for virtual preferences that exist only for
// api::settings_private. These are used to control Chrome Settings UI elements
// not directly attached to user preferences.
class GeneratedPrefs : public KeyedService {
 public:
  // Preference name to implementation map.
  using PrefsMap = base::flat_map<std::string, std::unique_ptr<GeneratedPref>>;

  explicit GeneratedPrefs(Profile* profile);

  GeneratedPrefs(const GeneratedPrefs&) = delete;
  GeneratedPrefs& operator=(const GeneratedPrefs&) = delete;

  ~GeneratedPrefs() override;

  // Returns true if preference is supported.
  bool HasPref(const std::string& pref_name);

  // Returns fully populated PrefObject or nullopt if not supported.
  std::optional<api::settings_private::PrefObject> GetPref(
      const std::string& pref_name);

  // Updates preference value.
  SetPrefResult SetPref(const std::string& pref_name, const base::Value* value);

  // Modify list of observers for the given preference.
  void AddObserver(const std::string& pref_name,
                   GeneratedPref::Observer* observer);
  void RemoveObserver(const std::string& pref_name,
                      GeneratedPref::Observer* observer);

  // KeyedService:
  void Shutdown() override;

 private:
  // Returns preference implementation or nullptr if not found. Will create
  // preferences if they haven't already been created.
  GeneratedPref* FindPrefImpl(const std::string& pref_name);

  // Creates all generated preferences and populates the preference map.
  void CreatePrefs();

  // Preference object map.
  PrefsMap prefs_;

  raw_ptr<Profile> profile_;
};

}  // namespace settings_private
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_GENERATED_PREFS_H_
