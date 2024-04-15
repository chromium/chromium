// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_GENERATED_PREF_TEST_BASE_H_
#define CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_GENERATED_PREF_TEST_BASE_H_

#include "chrome/browser/extensions/api/settings_private/generated_pref.h"
#include "chrome/common/extensions/api/settings_private.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace settings_api = extensions::api::settings_private;

namespace extensions {
namespace settings_private {

// All of the possible managed states for a boolean preference that can be
// both enforced and recommended.
enum class PrefSetting {
  kEnforcedOff,
  kEnforcedOn,
  kRecommendedOff,
  kRecommendedOn,
  kNotSet,
};

// Possible preference sources supported by TestingPrefService.
// TODO(crbug.com/40123235): Extend TestingPrefService to support prefs set for
//                          supervised users.
enum class PrefSource {
  kExtension,
  kDevicePolicy,
  kRecommended,
  kNone,
};

// Sets |pref_name| to |pref_setting|, using the appropriate store in |prefs|
// for |source|.
void SetPrefFromSource(sync_preferences::TestingPrefServiceSyncable* prefs,
                       const std::string& pref_name,
                       settings_private::PrefSetting pref_setting,
                       settings_private::PrefSource source);

class TestGeneratedPrefObserver : public GeneratedPref::Observer {
 public:
  void OnGeneratedPrefChanged(const std::string& pref_name) override;

  void Reset() { updated_pref_name_ = ""; }
  std::string GetUpdatedPrefName() { return updated_pref_name_; }

 protected:
  std::string updated_pref_name_;
};

class GeneratedPrefTestBase : public testing::Test {
 protected:
  TestingProfile* profile() { return &profile_; }

  sync_preferences::TestingPrefServiceSyncable* prefs() {
    return profile()->GetTestingPrefService();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

}  // namespace settings_private
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_GENERATED_PREF_TEST_BASE_H_
