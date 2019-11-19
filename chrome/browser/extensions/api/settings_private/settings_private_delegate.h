// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_SETTINGS_PRIVATE_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_SETTINGS_PRIVATE_DELEGATE_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/extensions/api/settings_private/prefs_util.h"
#include "chrome/common/extensions/api/settings_private.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/extension_function.h"

class Profile;

namespace base {
class Value;
}

namespace extensions {

using TypedPrefMap = std::map<std::string, api::settings_private::PrefType>;

// Manages all the pref service interactions.
// Use SettingsPrivateDelegateFactory to create a SettingsPrivateDelegate
// object.
class SettingsPrivateDelegate : public KeyedService {
 public:
  explicit SettingsPrivateDelegate(Profile* profile);
  ~SettingsPrivateDelegate() override;

  // Sets the pref with the given name and value in the proper PrefService.
  virtual settings_private::SetPrefResult SetPref(const std::string& name,
                                                  const base::Value* value);

  // Gets the value of the pref with the given |name|.
  virtual std::unique_ptr<base::Value> GetPref(const std::string& name);

  // Gets the values of all whitelisted prefs.
  virtual std::unique_ptr<base::Value> GetAllPrefs();

  // Gets the value.
  virtual std::unique_ptr<base::Value> GetDefaultZoom();

  // Sets the pref.
  virtual settings_private::SetPrefResult SetDefaultZoom(double zoom);

  Profile* profile_for_test() { return profile_; }

 protected:
  Profile* profile_;  // weak; not owned by us
  std::unique_ptr<PrefsUtil> prefs_util_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SettingsPrivateDelegate);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_SETTINGS_PRIVATE_DELEGATE_H_
