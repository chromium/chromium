// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_PREFS_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_PREFS_UTIL_H_

#include <map>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/settings_private/prefs_util_enums.h"
#include "chrome/common/extensions/api/settings_private.h"

class PrefService;
class Profile;

namespace extensions {
class Extension;

class PrefsUtil {

 public:
  // TODO(dbeam): why is the key a std::string rather than const char*?
  using TypedPrefMap = std::map<std::string, api::settings_private::PrefType>;

  explicit PrefsUtil(Profile* profile);
  virtual ~PrefsUtil();

  // Gets the list of allowlisted pref keys -- that is, those which correspond
  // to prefs that clients of the settingsPrivate API may retrieve and
  // manipulate.
  const TypedPrefMap& GetAllowlistedKeys();

  // Returns the pref type for |pref_name| or PREF_TYPE_NONE if not in the
  // allowlist.
  api::settings_private::PrefType GetAllowlistedPrefType(
      const std::string& pref_name);

  // Gets the value of the pref with the given |name|. Returns a nullopt if no
  // pref is found for |name|.
  virtual std::optional<api::settings_private::PrefObject> GetPref(
      const std::string& name);

  // Sets the pref with the given name and value in the proper PrefService.
  virtual settings_private::SetPrefResult SetPref(const std::string& name,
                                                  const base::Value* value);

  // Appends the given |value| to the list setting specified by the path in
  // |pref_name|.
  virtual bool AppendToListCrosSetting(const std::string& pref_name,
                                       const base::Value& value);

  // Removes the given |value| from the list setting specified by the path in
  // |pref_name|.
  virtual bool RemoveFromListCrosSetting(const std::string& pref_name,
                                         const base::Value& value);

  // Returns a pointer to the appropriate PrefService instance for the given
  // |pref_name|.
  virtual PrefService* FindServiceForPref(const std::string& pref_name);

  // Returns whether or not the given pref is a CrOS-specific setting.
  virtual bool IsCrosSetting(const std::string& pref_name);

 protected:
  // Returns whether |pref_name| corresponds to a pref whose type is URL.
  bool IsPrefTypeURL(const std::string& pref_name);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Returns whether |pref_name| corresponds to a pref that is enterprise
  // managed.
  bool IsPrefEnterpriseManaged(const std::string& pref_name);

  // Returns whether |pref_name| corresponds to a pref that is controlled by
  // the owner, and |profile_| is not the owner profile.
  bool IsPrefOwnerControlled(const std::string& pref_name);

  // Returns whether |pref_name| corresponds to a pref that is controlled by
  // the primary user, and |profile_| is not the primary profile.
  bool IsPrefPrimaryUserControlled(const std::string& pref_name);

  // Returns whether |pref_name| corresponds to the hotword enabled pref, if the
  // pref is disabled, and if |profile_| is a child user.
  bool IsHotwordDisabledForChildUser(const std::string& pref_name);
#endif

  // Returns whether |pref_name| corresponds to a pref that is controlled by
  // a supervisor, and |profile_| is supervised.
  bool IsPrefSupervisorControlled(const std::string& pref_name);

  // Returns whether |pref_name| corresponds to a pref that is user modifiable
  // (i.e., not made restricted by a user or device policy).
  bool IsPrefUserModifiable(const std::string& pref_name);

  api::settings_private::PrefType GetType(const std::string& name,
                                          base::Value::Type type);

  std::optional<api::settings_private::PrefObject> GetCrosSettingsPref(
      const std::string& name);

  settings_private::SetPrefResult SetCrosSettingsPref(const std::string& name,
                                                      const base::Value* value);

 private:
  const Extension* GetExtensionControllingPref(
      const api::settings_private::PrefObject& pref_object);

  raw_ptr<Profile> profile_;  // weak
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_PREFS_UTIL_H_
