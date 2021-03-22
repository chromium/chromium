// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/prefs_ash.h"

#include <memory>

#include "ash/public/cpp/ash_pref_names.h"
#include "base/callback_helpers.h"
#include "base/optional.h"
#include "base/test/bind.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
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

  base::Optional<base::Value> value_;
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

  PrefService* local_state() {
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

TEST_F(PrefsAshTest, LocalStatePrefs) {
  local_state()->SetBoolean(metrics::prefs::kMetricsReportingEnabled, false);
  PrefsAsh prefs_ash(profile_manager(), local_state());
  prefs_ash.OnPrimaryProfileReadyForTesting(CreateProfile());
  mojo::Remote<mojom::Prefs> prefs_remote;
  prefs_ash.BindReceiver(prefs_remote.BindNewPipeAndPassReceiver());
  mojom::PrefPath path = mojom::PrefPath::kMetricsReportingEnabled;

  // Get returns value.
  base::Value get_value;
  prefs_remote->GetPref(
      path, base::BindLambdaForTesting([&](base::Optional<base::Value> value) {
        get_value = std::move(*value);
      }));
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
  EXPECT_EQ(1, prefs_ash.observers_[path].size());

  // Multiple observers is ok.
  auto observer2 = std::make_unique<TestObserver>();
  prefs_remote->AddObserver(path,
                            observer2->receiver_.BindNewPipeAndPassRemote());
  prefs_remote.FlushForTesting();
  EXPECT_TRUE(observer2->value_->GetBool());
  EXPECT_EQ(2, prefs_ash.observers_[path].size());

  // Observer should be notified when value changes.
  local_state()->SetBoolean(metrics::prefs::kMetricsReportingEnabled, false);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(observer1->value_->GetBool());
  EXPECT_FALSE(observer2->value_->GetBool());

  // Disconnect should remove PrefChangeRegistrar.
  observer1.reset();
  prefs_remote.FlushForTesting();
  EXPECT_EQ(1, prefs_ash.observers_[path].size());
  observer2.reset();
  prefs_remote.FlushForTesting();
  EXPECT_EQ(0, prefs_ash.observers_[path].size());
  EXPECT_FALSE(prefs_ash.local_state_registrar_.IsObserved(
      metrics::prefs::kMetricsReportingEnabled));
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
  prefs_remote->GetPref(
      path, base::BindLambdaForTesting([&](base::Optional<base::Value> value) {
        get_value = std::move(*value);
      }));
  prefs_remote.FlushForTesting();
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

  // Get for an unknown value returns base::nullopt.
  bool has_value = true;
  prefs_remote->GetPref(
      path, base::BindLambdaForTesting([&](base::Optional<base::Value> value) {
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

}  // namespace crosapi
