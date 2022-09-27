// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_preload_service/app_preload_service.h"
#include <algorithm>
#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_preload_service/app_preload_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

static constexpr char kFirstLoginFlowCompletedKey[] =
    "first_login_flow_completed";

static constexpr char kApsStateManager[] =
    "apps.app_preload_service.state_manager";

const base::Value::Dict& GetStateManager(Profile* profile) {
  return profile->GetPrefs()->GetDict(kApsStateManager);
}

}  // namespace

namespace apps {

class AppPreloadServiceTest : public testing::Test {
 public:
  void VerifyFirstLoginPrefSet(base::OnceClosure on_complete) {
    // We expect that the key has been set after the first login flow has been
    // completed.
    auto flow_completed =
        GetStateManager(GetProfile()).FindBool(kFirstLoginFlowCompletedKey);
    EXPECT_NE(flow_completed, absl::nullopt);
    EXPECT_TRUE(flow_completed.value());

    std::move(on_complete).Run();
  }

 protected:
  AppPreloadServiceTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kAppPreloadService);
  }

  void SetUp() override {
    testing::Test::SetUp();

    profile_ = std::make_unique<TestingProfile>();
  }

  Profile* GetProfile() { return profile_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(AppPreloadServiceTest, ServiceAccessPerProfile) {
  // We expect the App Preload Service to be available in a normal profile.
  TestingProfile::Builder profile_builder;
  auto profile = profile_builder.Build();
  auto* service = AppPreloadServiceFactory::GetForProfile(profile.get());
  EXPECT_NE(nullptr, service);

  // The service is unsupported in incognito.
  TestingProfile::Builder incognito_builder;
  auto* incognito_profile = incognito_builder.BuildIncognito(profile.get());
  EXPECT_EQ(nullptr,
            AppPreloadServiceFactory::GetForProfile(incognito_profile));

  // We expect the App Preload Service to be available in a guest profile.
  TestingProfile::Builder guest_builder;
  guest_builder.SetGuestSession();
  auto guest_profile = guest_builder.Build();
  auto* guest_service =
      AppPreloadServiceFactory::GetForProfile(guest_profile.get());
  EXPECT_NE(nullptr, guest_service);

  // The service is not available for the OTR profile in guest mode.
  auto* guest_otr_profile =
      guest_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  EXPECT_EQ(nullptr,
            AppPreloadServiceFactory::GetForProfile(guest_otr_profile));

  // We expect a different service in the Guest Session profile.
  EXPECT_NE(guest_service, service);
}

TEST_F(AppPreloadServiceTest, FirstLoginPrefSet) {
  auto flow_completed =
      GetStateManager(GetProfile()).FindBool(kFirstLoginFlowCompletedKey);
  // Since we're creating a new profile with no saved state, we expect the
  // absence of the key.
  EXPECT_EQ(flow_completed, absl::nullopt);

  base::RunLoop run_loop;
  auto* service = AppPreloadService::Get(GetProfile());
  service->check_first_pref_set_callback_ =
      base::BindOnce(&AppPreloadServiceTest::VerifyFirstLoginPrefSet,
                     base::Unretained(this), run_loop.QuitClosure());

  run_loop.Run();
}

}  // namespace apps
