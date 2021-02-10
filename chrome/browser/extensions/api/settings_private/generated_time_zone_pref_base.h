// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_GENERATED_TIME_ZONE_PREF_BASE_H_
#define CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_GENERATED_TIME_ZONE_PREF_BASE_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/chromeos/system/timezone_resolver_manager.h"
#include "chrome/browser/extensions/api/settings_private/generated_pref.h"

class Profile;

namespace extensions {
namespace settings_private {

// Base class for several generated Time Zone preferences.
class GeneratedTimeZonePrefBase
    : public GeneratedPref,
      public chromeos::system::TimeZoneResolverManager::Observer {
 public:
  ~GeneratedTimeZonePrefBase() override;

  // chromeos::system::TimeZoneResolverManager::Observer
  void OnTimeZoneResolverUpdated() override;

 protected:
  GeneratedTimeZonePrefBase(const std::string& pref_name, Profile* profile);

  void UpdateTimeZonePrefControlledBy(
      api::settings_private::PrefObject* out_pref) const;

  const std::string pref_name_;

  Profile* const profile_;

  DISALLOW_COPY_AND_ASSIGN(GeneratedTimeZonePrefBase);
};

}  // namespace settings_private
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_GENERATED_TIME_ZONE_PREF_BASE_H_
