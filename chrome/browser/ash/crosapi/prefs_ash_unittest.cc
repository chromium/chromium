// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/prefs_ash.h"

#include <memory>
#include <optional>

#include "ash/constants/ash_pref_names.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crosapi {
namespace {

class TestObserver : public mojom::PrefObserver {
 public:
  TestObserver() = default;
  TestObserver(const TestObserver&) = delete;
  TestObserver& operator=(const TestObserver&) = delete;
  ~TestObserver() override = default;

  // crosapi::mojom::PrefObserver:
  void OnPrefChanged(base::Value value) override { value_ = std::move(value); }

  std::optional<base::Value> value_;
  mojo::Receiver<mojom::PrefObserver> receiver_{this};
};

}  // namespace

class PrefsAshTest : public testing::Test {
 public:
  PrefsAshTest() = default;
  PrefsAshTest(const PrefsAshTest&) = delete;
  PrefsAshTest& operator=(const PrefsAshTest&) = delete;
  ~PrefsAshTest() override = default;

  void SetUp() override { ASSERT_TRUE(testing_profile_manager_.SetUp()); }

  TestingPrefServiceSimple* local_state() {
    return testing_profile_manager_.local_state()->Get();
  }
  ProfileManager* profile_manager() {
    return testing_profile_manager_.profile_manager();
  }

  Profile* CreateProfile() {
    return testing_profile_manager_.CreateTestingProfile(std::string());
  }

  content::BrowserTaskEnvironment task_environment_;

 private:
  TestingProfileManager testing_profile_manager_{
      TestingBrowserProcess::GetGlobal()};
};

void GetExtensionPrefWithControl(mojo::Remote<mojom::Prefs>& prefs_remote,
                                 mojom::PrefPath path,
                                 base::Value* get_value,
                                 mojom::PrefControlState* get_control) {
  prefs_remote->GetExtensionPrefWithControl(
      path, base::BindLambdaForTesting([&](std::optional<base::Value> value,
                                           mojom::PrefControlState control) {
        *get_value = std::move(*value);
        *get_control = control;
      }));
  prefs_remote.FlushForTesting();
}

void GetPref(mojo::Remote<mojom::Prefs>& prefs_remote,
             mojom::PrefPath path,
             base::Value* get_value) {
  prefs_remote->GetPref(
      path, base::BindLambdaForTesting([&](std::optional<base::Value> value) {
        *get_value = std::move(*value);
      }));
  prefs_remote.FlushForTesting();
}

TEST_F(PrefsAshTest, LocalStatePrefs) {
  local_state()->SetBoolean(metrics::prefs::kMetricsReportingEnabled, false);
  PrefsAsh prefs_ash(profile_manager(), local_state());
  prefs_ash.OnPrimaryProfileReadyForTesting(CreateProfile());
  mojo::Remote<mojom::Prefs> prefs_remote;
  prefs_ash.BindReceiver(prefs_remote.BindNewPipeAndPassReceiver());
  mojom::PrefPath path = mojom::PrefPath::kMetricsReportingEnabled;

  // Get returns value.
  base::Value get_value;
  GetPref(prefs_remote, path, &get_value);
  prefs_remote.FlushForTesting();
  EXPECT_FALSE(get_value.GetBool());

  // Set updates value.
  prefs_remote->SetPref(path, base::Value(true), base::DoNothing());
  prefs_remote.FlushForTesting();
  EXPECT_TRUE(
      local_state()->GetBoolean(metrics::prefs::kMetricsReportingEnabled));

  // Adding an observer results in it being fired with the current state.
  EXPECT_FALSE(prefs_ash.local_state_registrar_.IsObserved(
      metrics::prefs::kMetricsReportingEnabled));
  auto observer1 = std::make_unique<TestObserver>();
  prefs_remote->AddObserver(path,
                            observer1->receiver_.BindNewPipeAndPassRemote());
  prefs_remote.FlushForTesting();
  EXPECT_TRUE(observer1->value_->GetBool());
  EXPECT_TRUE(prefs_ash.local_state_registrar_.IsObserved(
      metrics::prefs::kMetricsReportingEnabled));
  EXPECT_EQ(1u, prefs_ash.observers_[path].size());

  // Multiple observers is ok.
  auto observer2 = std::make_unique<TestObserver>();
  prefs_remote->AddObserver(path,
                            observer2->receiver_.BindNewPipeAndPassRemote());
  prefs_remote.FlushForTesting();
  EXPECT_TRUE(observer2->value_->GetBool());
  EXPECT_EQ(2u, prefs_ash.observers_[path].size());

  // Observer should be notified when value changes.
  local_state()->SetBoolean(metrics::prefs::kMetricsReportingEnabled, false);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(observer1->value_->GetBool());
  EXPECT_FALSE(observer2->value_->GetBool());

  // Disconnect should remove PrefChangeRegistrar.
  observer1.reset();
  prefs_remote.FlushForTesting();
  EXPECT_EQ(1u, prefs_ash.observers_[path].size());
  observer2.reset();
  prefs_remote.FlushForTesting();
  EXPECT_EQ(0u, prefs_ash.observers_[path].size());
  EXPECT_FALSE(prefs_ash.local_state_registrar_.IsObserved(
      metrics::prefs::kMetricsReportingEnabled));
}

TEST_F(PrefsAshTest, LocalStatePref_SystemTracing) {
  const char* pref_name = ash::prefs::kDeviceSystemWideTracingEnabled;

  PrefsAsh prefs_ash(profile_manager(), local_state());
  prefs_ash.OnPrimaryProfileReadyForTesting(CreateProfile());
  mojo::Remote<mojom::Prefs> prefs_remote;
  prefs_ash.BindReceiver(prefs_remote.BindNewPipeAndPassReceiver());
  mojom::PrefPath path = mojom::PrefPath::kDeviceSystemWideTracingEnabled;

  // Get returns value.
  base::Value get_value;
  // Tests the default pref value.
  GetPref(prefs_remote, path, &get_value);
  EXPECT_TRUE(get_value.GetBool());

  // Tests the GetPref() method.
  local_state()->SetBoolean(pref_name, false);
  GetPref(prefs_remote, path, &get_value);
  prefs_remote.FlushForTesting();
  EXPECT_FALSE(get_value.GetBool());

  local_state()->SetBoolean(pref_name, true);
  GetPref(prefs_remote, path, &get_value);
  EXPECT_TRUE(get_value.GetBool());

  // Tests observing pref changes.
  auto observer = std::make_unique<TestObserver>();
  prefs_remote->AddObserver(path,
                            observer->receiver_.BindNewPipeAndPassRemote());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(observer->value_->GetBool());

  local_state()->SetBoolean(pref_name, false);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(observer->value_->GetBool());
}

TEST_F(PrefsAshTest, ProfilePrefs) {
  Profile* const profile = CreateProfile();
  PrefService* const profile_prefs = profile->GetPrefs();
  profile_prefs->SetBoolean(ash::prefs::kAccessibilitySpokenFeedbackEnabled,
                            false);
  PrefsAsh prefs_ash(profile_manager(), local_state());
  prefs_ash.OnPrimaryProfileReadyForTesting(profile);

  mojo::Remote<mojom::Prefs> prefs_remote;
  prefs_ash.BindReceiver(prefs_remote.BindNewPipeAndPassReceiver());
  mojom::PrefPath path = mojom::PrefPath::kAccessibilitySpokenFeedbackEnabled;

  // Get returns value.
  base::Value get_value;
  GetPref(prefs_remote, path, &get_value);
  EXPECT_FALSE(get_value.GetBool());

  // Set updates value.
  prefs_remote->SetPref(path, base::Value(true), base::DoNothing());
  prefs_remote.FlushForTesting();
  EXPECT_TRUE(profile_prefs->GetBoolean(
      ash::prefs::kAccessibilitySpokenFeedbackEnabled));

  // Adding an observer results in it being fired with the current state.
  TestObserver observer;
  prefs_remote->AddObserver(path,
                            observer.receiver_.BindNewPipeAndPassRemote());
  prefs_remote.FlushForTesting();
  EXPECT_TRUE(observer.value_->GetBool());
}

TEST_F(PrefsAshTest, GetUnknown) {
  PrefsAsh prefs_ash(profile_manager(), local_state());
  prefs_ash.OnPrimaryProfileReadyForTesting(CreateProfile());
  mojo::Remote<mojom::Prefs> prefs_remote;
  prefs_ash.BindReceiver(prefs_remote.BindNewPipeAndPassReceiver());
  mojom::PrefPath path = mojom::PrefPath::kUnknown;

  // Get for an unknown value returns std::nullopt.
  bool has_value = true;
  prefs_remote->GetPref(
      path, base::BindLambdaForTesting([&](std::optional<base::Value> value) {
        has_value = value.has_value();
      }));
  prefs_remote.FlushForTesting();
  EXPECT_FALSE(has_value);

  // Set or AddObserver for an unknown value does nothing.
  prefs_remote->SetPref(path, base::Value(false), base::DoNothing());
  TestObserver observer;
  prefs_remote->AddObserver(path,
                            observer.receiver_.BindNewPipeAndPassRemote());
  prefs_remote.FlushForTesting();
  EXPECT_FALSE(observer.value_.has_value());
}

TEST_F(PrefsAshTest, GetWithControlUnknown) {
  Profile* const profile = CreateProfile();
  PrefsAsh prefs_ash(profile_manager(), local_state());
  prefs_ash.OnPrimaryProfileReadyForTesting(profile);
  mojo::Remote<mojom::Prefs> prefs_remote;
  prefs_ash.BindReceiver(prefs_remote.BindNewPipeAndPassReceiver());
  mojom::PrefPath path = mojom::PrefPath::kUnknown;

  // Get for an unknown value returns std::nullopt.
  bool has_value = true;
  mojom::PrefControlState get_control;
  prefs_remote->GetExtensionPrefWithControl(
      path, base::BindLambdaForTesting([&](std::optional<base::Value> value,
                                           mojom::PrefControlState control) {
        has_value = value.has_value();
        get_control = control;
      }));
  prefs_remote.FlushForTesting();
  EXPECT_FALSE(has_value);
  EXPECT_EQ(get_control, mojom::PrefControlState::kDefaultUnknown);
}

TEST_F(PrefsAshTest, ExtensionPrefsControllable) {
  local_state()->registry()->RegisterBooleanPref(
      ash::prefs::kDockedMagnifierEnabled, false);

  Profile* const profile = CreateProfile();
  PrefService* const profile_prefs = profile->GetPrefs();
  PrefsAsh prefs_ash(profile_manager(), local_state());
  prefs_ash.OnPrimaryProfileReadyForTesting(profile);

  profile_prefs->SetBoolean(ash::prefs::kDockedMagnifierEnabled, true);

  mojo::Remote<mojom::Prefs> prefs_remote;
  prefs_ash.BindReceiver(prefs_remote.BindNewPipeAndPassReceiver());
  mojom::PrefPath path = mojom::PrefPath::kDockedMagnifierEnabled;

  // Get returns value.
  base::Value get_value;
  GetPref(prefs_remote, path, &get_value);
  EXPECT_TRUE(get_value.GetBool());

  // GetWithControl shows this can be controlled by lacros because extensions
  // have higher precedence than profile prefs.
  mojom::PrefControlState get_control;
  GetExtensionPrefWithControl(prefs_remote, path, &get_value, &get_control);

  EXPECT_EQ(get_control, mojom::PrefControlState::kLacrosExtensionControllable);
  EXPECT_TRUE(get_value.GetBool());
}

TEST_F(PrefsAshTest, ExtensionPrefsGetSetClear) {
  local_state()->registry()->RegisterBooleanPref(
      ash::prefs::kDockedMagnifierEnabled, false);

  Profile* const profile = CreateProfile();
  PrefsAsh prefs_ash(profile_manager(), local_state());
  prefs_ash.OnPrimaryProfileReadyForTesting(profile);

  mojo::Remote<mojom::Prefs> prefs_remote;
  prefs_ash.BindReceiver(prefs_remote.BindNewPipeAndPassReceiver());
  mojom::PrefPath path = mojom::PrefPath::kDockedMagnifierEnabled;

  prefs_remote->SetPref(path, base::Value(true), base::DoNothing());
  prefs_remote.FlushForTesting();

  base::Value get_value;
  mojom::PrefControlState get_control;

  GetExtensionPrefWithControl(prefs_remote, path, &get_value, &get_control);

  // Controlled by lacros as it was set above.
  EXPECT_EQ(get_control, mojom::PrefControlState::kLacrosExtensionControlled);
  EXPECT_TRUE(get_value.GetBool());

  // Clear Extension controlled pref.
  prefs_remote->ClearExtensionControlledPref(path, base::DoNothing());
  prefs_remote.FlushForTesting();

  GetExtensionPrefWithControl(prefs_remote, path, &get_value, &get_control);

  // Controllable by lacros, as it was unset above. No longer enabled as it
  // was cleared.
  EXPECT_EQ(get_control, mojom::PrefControlState::kLacrosExtensionControllable);
  EXPECT_FALSE(get_value.GetBool());
}

TEST_F(PrefsAshTest, ExtensionPrefsClearNonExtensionPref) {
  local_state()->registry()->RegisterBooleanPref(
      ash::prefs::kAccessibilitySpokenFeedbackEnabled, false);

  Profile* const profile = CreateProfile();
  PrefService* const profile_prefs = profile->GetPrefs();
  PrefsAsh prefs_ash(profile_manager(), local_state());
  prefs_ash.OnPrimaryProfileReadyForTesting(profile);

  profile_prefs->SetBoolean(ash::prefs::kAccessibilitySpokenFeedbackEnabled,
                            true);

  mojo::Remote<mojom::Prefs> prefs_remote;
  prefs_ash.BindReceiver(prefs_remote.BindNewPipeAndPassReceiver());
  // Note this is the non-extension PrefPath.
  mojom::PrefPath path = mojom::PrefPath::kAccessibilitySpokenFeedbackEnabled;

  // Does nothing since this is not an extension controlled pref.
  prefs_remote->ClearExtensionControlledPref(path, base::DoNothing());
  prefs_remote.FlushForTesting();

  // Get returns value.
  base::Value get_value;
  GetPref(prefs_remote, path, &get_value);
  EXPECT_TRUE(get_value.GetBool());
}

TEST_F(PrefsAshTest, CrosSettingsPrefs) {
  ash::ScopedTestingCrosSettings scoped_testing_cros_settings;

  PrefsAsh prefs_ash(profile_manager(), local_state());
  mojo::Remote<mojom::Prefs> prefs_remote;
  prefs_ash.BindReceiver(prefs_remote.BindNewPipeAndPassReceiver());
  mojom::PrefPath path =
      mojom::PrefPath::kAttestationForContentProtectionEnabled;

  // Get returns value, which defaults to true.
  base::Value get_value;
  GetPref(prefs_remote, path, &get_value);
  prefs_remote.FlushForTesting();
  EXPECT_TRUE(get_value.GetBool());

  // Set does not update values for CrosSettings.
  prefs_remote->SetPref(path, base::Value(false), base::DoNothing());
  prefs_remote.FlushForTesting();
  EXPECT_TRUE(scoped_testing_cros_settings.device_settings()
                  ->Get(ash::kAttestationForContentProtectionEnabled)
                  ->GetBool());

  // Adding an observer results in it being fired with the current state.
  auto observer1 = std::make_unique<TestObserver>();
  prefs_remote->AddObserver(path,
                            observer1->receiver_.BindNewPipeAndPassRemote());
  prefs_remote.FlushForTesting();
  EXPECT_TRUE(observer1->value_->GetBool());
  EXPECT_EQ(1u, prefs_ash.cros_settings_subs_.count(path));
  EXPECT_EQ(1u, prefs_ash.observers_[path].size());

  // Multiple observers is ok.
  auto observer2 = std::make_unique<TestObserver>();
  prefs_remote->AddObserver(path,
                            observer2->receiver_.BindNewPipeAndPassRemote());
  prefs_remote.FlushForTesting();
  EXPECT_TRUE(observer2->value_->GetBool());
  EXPECT_EQ(2u, prefs_ash.observers_[path].size());

  // Observer should be notified when value changes.
  scoped_testing_cros_settings.device_settings()->SetBoolean(
      ash::kAttestationForContentProtectionEnabled, false);
  task_environment_.RunUntilIdle();
  prefs_remote.FlushForTesting();
  EXPECT_FALSE(observer1->value_->GetBool());
  EXPECT_FALSE(observer2->value_->GetBool());

  // Disconnect should remove CallbackListSubscription.
  observer1.reset();
  prefs_remote.FlushForTesting();
  EXPECT_EQ(1u, prefs_ash.observers_[path].size());
  observer2.reset();
  prefs_remote.FlushForTesting();
  EXPECT_EQ(0u, prefs_ash.observers_[path].size());
  EXPECT_EQ(0u, prefs_ash.cros_settings_subs_.count(path));
}

}  // namespace crosapi
