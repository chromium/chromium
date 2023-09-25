// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/sync/test/integration/web_apps_sync_test_base.h"
#include "chrome/browser/web_applications/generated_icon_fix_manager.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/chrome_features.h"
#include "content/public/test/browser_test.h"

namespace web_app {

namespace {

struct GeneratedIconFixFutures {
  base::test::TestFuture<const webapps::AppId&,
                         GeneratedIconFixScheduleDecision>
      schedule;
  base::test::TestFuture<const webapps::AppId&, GeneratedIconFixResult> fix;

  explicit GeneratedIconFixFutures(FakeWebAppProvider& provider) {
    GeneratedIconFixManager& generated_icon_fix_manager =
        provider.generated_icon_fix_manager();
    generated_icon_fix_manager.maybe_schedule_callback_for_testing() =
        schedule.GetCallback();
    generated_icon_fix_manager.fix_completed_callback_for_testing() =
        fix.GetCallback();
  }
};

}  // namespace

class TwoClientGeneratedIconFixSyncTest : public WebAppsSyncTestBase {
 public:
  TwoClientGeneratedIconFixSyncTest() : WebAppsSyncTestBase(TWO_CLIENT) {}
  ~TwoClientGeneratedIconFixSyncTest() override = default;

  void SetUpOnMainThread() override {
    WebAppsSyncTestBase::SetUpOnMainThread();

    ASSERT_TRUE(SetupSync());
  }

  void TearDownOnMainThread() override {
    fake_providers_.clear();
    WebAppsSyncTestBase::TearDownOnMainThread();
  }

  webapps::AppId SyncBrokenIcon(Profile* source, Profile* destination) {
    WebAppTestInstallObserver install_observer{destination};

    // Install on source profile.
    auto info = std::make_unique<WebAppInstallInfo>();
    info->title = u"Test name";
    info->description = u"Test description";
    info->start_url = GURL("https://example.com");
    info->manifest_icons.emplace_back(
        apps::IconInfo(GURL("https://example.com/icon.png"), 256));
    webapps::AppId app_id = test::InstallWebApp(source, std::move(info));

    // Wait for sync install on destination profile.
    install_observer.BeginListening({app_id});
    install_observer.Wait();

#if !BUILDFLAG(IS_CHROMEOS)
    // Install locally on destination profile.
    base::test::TestFuture<void> install_locally_future;
    fake_providers_[destination]->scheduler().InstallAppLocally(
        app_id, install_locally_future.GetCallback());
    install_locally_future.Wait();
#endif  // !BUILDFLAG(IS_CHROMEOS)

    return app_id;
  }

  struct IconState {
    bool is_generated;
    bool is_correct_color;
    bool operator==(const IconState& other) const {
      return is_generated == other.is_generated &&
             is_correct_color == other.is_correct_color;
    }
  };
  IconState CheckIconState(Profile* profile, const webapps::AppId& app_id) {
    base::test::TestFuture<std::map<SquareSizePx, SkBitmap>> icons_future;
    fake_providers_[profile]->icon_manager().ReadIcons(
        app_id, IconPurpose::ANY, {256}, icons_future.GetCallback());
    return {
        .is_generated = fake_providers_[profile]
                            ->registrar_unsafe()
                            .GetAppById(app_id)
                            ->is_generated_icon(),
        .is_correct_color =
            icons_future.Get<0>().at(256).getColor(100, 100) == SK_ColorBLUE,
    };
  }

  void EnableIconServing(Profile* profile) {
    auto& fake_web_contents_manager = static_cast<FakeWebContentsManager&>(
        fake_providers_[profile]->web_contents_manager());
    FakeWebContentsManager::FakeIconState icon_state;
    icon_state.bitmaps.emplace_back(CreateSquareIcon(256, SK_ColorBLUE));
    fake_web_contents_manager.SetIconState(GURL("https://example.com/icon.png"),
                                           icon_state);
  }

 protected:
  FakeWebAppProviderCreator fake_web_app_provider_creator_{
      base::BindLambdaForTesting(
          [this](Profile* profile) -> std::unique_ptr<KeyedService> {
            auto fake_provider = std::make_unique<FakeWebAppProvider>(profile);

            fake_provider->CreateFakeSubsystems();
            // Use the real sync bridge to integrate with real sync logic.
            fake_provider->SetSyncBridge(std::make_unique<WebAppSyncBridge>(
                &fake_provider->GetRegistrarMutable()));
            fake_provider->StartWithSubsystems();

            fake_providers_[profile] = fake_provider.get();

            return fake_provider;
          })};

  base::flat_map<raw_ptr<Profile>, raw_ptr<FakeWebAppProvider>> fake_providers_;

  OsIntegrationManager::ScopedSuppressForTesting os_hooks_suppress_;

  base::test::ScopedFeatureList scoped_feature_list_{
      features::kWebAppSyncGeneratedIconBackgroundFix};
};

IN_PROC_BROWSER_TEST_F(TwoClientGeneratedIconFixSyncTest, Fix) {
  webapps::AppId app_id = SyncBrokenIcon(GetProfile(0), GetProfile(1));

  EXPECT_EQ(CheckIconState(GetProfile(1), app_id),
            (IconState{.is_generated = true, .is_correct_color = false}));

  EnableIconServing(GetProfile(1));

  FakeWebAppProvider& provider1 = *fake_providers_[GetProfile(1)];
  GeneratedIconFixFutures futures(provider1);

  // Simulate a restart (it's difficult to set up the callback listener in
  // time for a real restart).
  provider1.generated_icon_fix_manager().Start();

  EXPECT_EQ(futures.schedule.Get<webapps::AppId>(), app_id);
  EXPECT_EQ(futures.schedule.Get<GeneratedIconFixScheduleDecision>(),
            GeneratedIconFixScheduleDecision::kSchedule);
  EXPECT_EQ(futures.fix.Get<webapps::AppId>(), app_id);
  EXPECT_EQ(futures.fix.Get<GeneratedIconFixResult>(),
            GeneratedIconFixResult::kSuccess);

  EXPECT_EQ(CheckIconState(GetProfile(1), app_id),
            (IconState{.is_generated = false, .is_correct_color = true}));
}

IN_PROC_BROWSER_TEST_F(TwoClientGeneratedIconFixSyncTest, TimeWindowExpired) {
  webapps::AppId app_id = SyncBrokenIcon(GetProfile(0), GetProfile(1));

  EXPECT_EQ(CheckIconState(GetProfile(1), app_id),
            (IconState{.is_generated = true, .is_correct_color = false}));

  EnableIconServing(GetProfile(1));

  FakeWebAppProvider& provider1 = *fake_providers_[GetProfile(1)];
  GeneratedIconFixFutures futures(provider1);

  // Simulate one week passing.
  provider1.generated_icon_fix_manager().time_for_testing() =
      base::Time::Now() + base::Days(7);
  // Simulate a restart (it's difficult to set up the callback listener in
  // time for a real restart).
  provider1.generated_icon_fix_manager().Start();

  EXPECT_EQ(futures.schedule.Get<webapps::AppId>(), app_id);
  EXPECT_EQ(futures.schedule.Get<GeneratedIconFixScheduleDecision>(),
            GeneratedIconFixScheduleDecision::kTimeWindowExpired);
  EXPECT_FALSE(provider1.generated_icon_fix_manager()
                   .scheduled_fixes_for_testing()
                   .contains(app_id));

  EXPECT_EQ(CheckIconState(GetProfile(1), app_id),
            (IconState{.is_generated = true, .is_correct_color = false}));
}

IN_PROC_BROWSER_TEST_F(TwoClientGeneratedIconFixSyncTest, NotRequired) {
  EnableIconServing(GetProfile(1));

  webapps::AppId app_id = SyncBrokenIcon(GetProfile(0), GetProfile(1));

  EXPECT_EQ(CheckIconState(GetProfile(1), app_id),
            (IconState{.is_generated = false, .is_correct_color = true}));

  FakeWebAppProvider& provider1 = *fake_providers_[GetProfile(1)];
  GeneratedIconFixFutures futures(provider1);

  // Simulate a restart (it's difficult to set up the callback listener in
  // time for a real restart).
  provider1.generated_icon_fix_manager().Start();

  EXPECT_EQ(futures.schedule.Get<webapps::AppId>(), app_id);
  EXPECT_EQ(futures.schedule.Get<GeneratedIconFixScheduleDecision>(),
            GeneratedIconFixScheduleDecision::kNotRequired);
  EXPECT_FALSE(provider1.generated_icon_fix_manager()
                   .scheduled_fixes_for_testing()
                   .contains(app_id));
}

IN_PROC_BROWSER_TEST_F(TwoClientGeneratedIconFixSyncTest, AppUninstalled) {
  webapps::AppId app_id = SyncBrokenIcon(GetProfile(0), GetProfile(1));

  EXPECT_EQ(CheckIconState(GetProfile(1), app_id),
            (IconState{.is_generated = true, .is_correct_color = false}));

  FakeWebAppProvider& provider1 = *fake_providers_[GetProfile(1)];
  GeneratedIconFixFutures futures(provider1);

  // Simulate a restart (it's difficult to set up the callback listener in time
  // for a real restart).
  provider1.generated_icon_fix_manager().Start();

  EXPECT_EQ(futures.schedule.Get<webapps::AppId>(), app_id);
  EXPECT_EQ(futures.schedule.Get<GeneratedIconFixScheduleDecision>(),
            GeneratedIconFixScheduleDecision::kSchedule);

  // Delete the app before the command gets a chance to run.
  // Just delete right out of the registrar because injecting into the
  // command queue is difficult.
  provider1.sync_bridge_unsafe().BeginUpdate()->DeleteApp(app_id);

  EXPECT_EQ(futures.fix.Get<webapps::AppId>(), app_id);
  EXPECT_EQ(futures.fix.Get<GeneratedIconFixResult>(),
            GeneratedIconFixResult::kAppUninstalled);
}

}  // namespace web_app
