// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_GENERATED_TIME_ZONE_PREF_BASE_H_
#define CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_GENERATED_TIME_ZONE_PREF_BASE_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/system/timezone_resolver_manager.h"
#include "chrome/browser/extensions/api/settings_private/generated_pref.h"

class Profile;

namespace extensions {
namespace settings_private {

// Returns true if the user associated with `profile` is allowed to change the
// timezone (either because the user is not a child user, or because parental
// approval has been granted for the user).
bool IsTimezoneChangeAllowed(Profile* profile);

// Base class for several generated Time Zone preferences.
class GeneratedTimeZonePrefBase
    : public GeneratedPref,
      public ash::system::TimeZoneResolverManager::Observer {
 public:
  GeneratedTimeZonePrefBase(const GeneratedTimeZonePrefBase&) = delete;
  GeneratedTimeZonePrefBase& operator=(const GeneratedTimeZonePrefBase&) =
      delete;

  ~GeneratedTimeZonePrefBase() override;

  // ash::system::TimeZoneResolverManager::Observer
  void OnTimeZoneResolverUpdated() override;

 protected:
  GeneratedTimeZonePrefBase(const std::string& pref_name, Profile* profile);

  void UpdateTimeZonePrefControlledBy(
      api::settings_private::PrefObject* out_pref) const;

  bool IsTimeZonePrefModifiable() const;

  const std::string pref_name_;

  const raw_ptr<Profile> profile_;
};

}  // namespace settings_private
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_GENERATED_TIME_ZONE_PREF_BASE_H_
