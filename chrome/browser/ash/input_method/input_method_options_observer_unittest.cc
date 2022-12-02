// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/input_method_options_observer.h"

#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::input_method {
namespace {

void SetAutocorrectLevel(PrefService* pref_service, int autocorrect_level) {
  ScopedDictPrefUpdate(pref_service,
                       prefs::kLanguageInputMethodSpecificSettings)
      ->SetByDottedPath("xkb:us::eng.physicalKeyboardAutoCorrectionLevel",
                        base::Value(autocorrect_level));
}

void SetPredictiveWriting(PrefService* pref_service, bool enabled) {
  ScopedDictPrefUpdate(pref_service,
                       prefs::kLanguageInputMethodSpecificSettings)
      ->SetByDottedPath("xkb:us::eng.physicalKeyboardEnabledPredictiveWriting",
                        base::Value(enabled));
}

class InputMethodOptionsObserverTest : public ::testing::Test {
 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(InputMethodOptionsObserverTest,
       InvokesCallbackWheneverSettingsAreChanged) {
  int num_calls_received = 0;
  PrefService* pref_service = profile_.GetPrefs();
  InputMethodOptionsObserver observer(pref_service);
  observer.Observe(base::BindLambdaForTesting(
      [&](const std::string& pref_path) { num_calls_received++; }));

  // Start making changes to the preferences that should be observed.
  SetAutocorrectLevel(pref_service, /*autocorrect_level=*/0);
  SetAutocorrectLevel(pref_service, /*autocorrect_level=*/1);
  SetAutocorrectLevel(pref_service, /*autocorrect_level=*/2);
  SetPredictiveWriting(pref_service, /*enabled=*/false);
  SetPredictiveWriting(pref_service, /*enabled=*/true);

  EXPECT_EQ(num_calls_received, 5);
}

TEST_F(InputMethodOptionsObserverTest,
       DoesNotInvokeCallbackWhenIrrelevantSettingsAreChanged) {
  int num_calls_received = 0;
  PrefService* pref_service = profile_.GetPrefs();
  InputMethodOptionsObserver observer(pref_service);
  observer.Observe(base::BindLambdaForTesting(
      [&](const std::string& pref_path) { num_calls_received++; }));

  // Update some random preferences that should not be observed.
  pref_service->Set(prefs::kLabsAdvancedFilesystemEnabled, base::Value(true));
  pref_service->Set(prefs::kLabsAdvancedFilesystemEnabled, base::Value(false));
  pref_service->Set(prefs::kShowMobileDataNotification, base::Value(true));
  pref_service->Set(prefs::kShowMobileDataNotification, base::Value(false));

  EXPECT_EQ(num_calls_received, 0);
}

TEST_F(InputMethodOptionsObserverTest,
       StopsObservingWhenObserverFallsOutOfScope) {
  int num_calls_received = 0;
  PrefService* pref_service = profile_.GetPrefs();

  {
    InputMethodOptionsObserver observer(pref_service);
    observer.Observe(base::BindLambdaForTesting(
        [&](const std::string& pref_path) { num_calls_received++; }));
  }

  // Start making changes to the preferences that should be observed.
  SetAutocorrectLevel(pref_service, /*autocorrect_level=*/0);
  SetAutocorrectLevel(pref_service, /*autocorrect_level=*/1);
  SetAutocorrectLevel(pref_service, /*autocorrect_level=*/2);
  SetPredictiveWriting(pref_service, /*enabled=*/false);
  SetPredictiveWriting(pref_service, /*enabled=*/true);

  EXPECT_EQ(num_calls_received, 0);
}

}  // namespace
}  // namespace ash::input_method
