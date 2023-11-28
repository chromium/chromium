// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_FAKE_ANDROID_INTENT_HELPER_H_
#define ASH_TEST_FAKE_ANDROID_INTENT_HELPER_H_

#include <map>
#include <optional>
#include <string>

#include "ash/public/cpp/android_intent_helper.h"

namespace ash {

// Fake instance of the |AndroidIntentHelper| used for test purposes.
// Will be installed as the singleton when constructed, and allows the injection
// of fake Android apps.
class FakeAndroidIntentHelper : public AndroidIntentHelper {
 public:
  using LocalizedAppName = std::string;
  using Intent = std::string;

  FakeAndroidIntentHelper();
  ~FakeAndroidIntentHelper() override;
  FakeAndroidIntentHelper(FakeAndroidIntentHelper&) = delete;
  FakeAndroidIntentHelper& operator=(FakeAndroidIntentHelper&) = delete;

  // AndroidIntentHelper overrides:
  void LaunchAndroidIntent(const std::string& intent) override;
  std::optional<std::string> GetAndroidAppLaunchIntent(
      const assistant::AndroidAppInfo& app_info) override;

  // Adds a fake Android app.
  // |intent| will be returned from GetAndroidAppLaunchIntent() if the value of
  // |AndroidAppInfo::localized_app_name| matches |name|.
  void AddApp(const LocalizedAppName& name, const Intent& intent);

  // Returns the intent of the last Android app that was launched.
  const std::optional<Intent>& last_launched_android_intent() const {
    return last_launched_intent_;
  }

 private:
  std::map<LocalizedAppName, Intent> apps_;
  std::optional<Intent> last_launched_intent_;
};

}  // namespace ash

#endif  // ASH_TEST_FAKE_ANDROID_INTENT_HELPER_H_
