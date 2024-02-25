// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_SETTINGS_PRIVATE_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_SETTINGS_PRIVATE_DELEGATE_H_

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/settings_private/prefs_util.h"
#include "chrome/common/extensions/api/settings_private.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/extension_function.h"

class Profile;

namespace extensions {

using TypedPrefMap = std::map<std::string, api::settings_private::PrefType>;

// Manages all the pref service interactions.
// Use SettingsPrivateDelegateFactory to create a SettingsPrivateDelegate
// object.
class SettingsPrivateDelegate : public KeyedService {
 public:
  explicit SettingsPrivateDelegate(Profile* profile);

  SettingsPrivateDelegate(const SettingsPrivateDelegate&) = delete;
  SettingsPrivateDelegate& operator=(const SettingsPrivateDelegate&) = delete;

  ~SettingsPrivateDelegate() override;

  // Sets the pref with the given name and value in the proper PrefService.
  virtual settings_private::SetPrefResult SetPref(const std::string& name,
                                                  const base::Value* value);

  // Gets the value of the pref with the given |name|.
  std::optional<base::Value::Dict> GetPref(const std::string& name);

  // Gets the values of all allowlisted prefs.
  virtual base::Value::List GetAllPrefs();

  // Gets the value.
  virtual base::Value GetDefaultZoom();

  // Sets the pref.
  virtual settings_private::SetPrefResult SetDefaultZoom(double zoom);

  Profile* profile_for_test() { return profile_; }

 protected:
  raw_ptr<Profile> profile_;  // weak; not owned by us
  std::unique_ptr<PrefsUtil> prefs_util_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_SETTINGS_PRIVATE_DELEGATE_H_
