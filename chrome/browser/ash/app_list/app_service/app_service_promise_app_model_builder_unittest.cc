// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/app_service/app_service_promise_app_model_builder.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_registry_cache.h"
#include "chrome/browser/ash/app_list/app_list_test_util.h"
#include "chrome/browser/ash/app_list/chrome_app_list_item.h"
#include "chrome/browser/ash/app_list/internal_app/internal_app_metadata.h"
#include "chrome/browser/ash/app_list/test/fake_app_list_model_updater.h"
#include "chrome/browser/ash/app_list/test/test_app_list_controller_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/package_id.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/test/test_screen.h"

class AppServicePromiseAppModelBuilderTest : public app_list::AppListTestBase {
 public:
  AppServicePromiseAppModelBuilderTest() = default;
  AppServicePromiseAppModelBuilderTest(
      const AppServicePromiseAppModelBuilderTest&) = delete;
  AppServicePromiseAppModelBuilderTest& operator=(
      const AppServicePromiseAppModelBuilderTest&) = delete;

  void TearDown() override {
    ResetBuilder();
    AppListTestBase::TearDown();
  }

 protected:
  void ResetBuilder() {
    scoped_callback_.reset();
    builder_.reset();
    controller_.reset();
    model_updater_.reset();
  }

  // Creates a new builder, destroying any existing one.
  void CreateBuilder(bool guest_mode) {
    ResetBuilder();  // Destroy any existing builder in the correct order.
    scoped_feature_list_.InitAndEnableFeature(ash::features::kPromiseIcons);
    testing_profile()->SetGuestSession(guest_mode);
    app_service_test_.SetUp(profile());
    model_updater_ = std::make_unique<FakeAppListModelUpdater>(
        /*profile=*/nullptr, /*reorder_delegate=*/nullptr);
    controller_ = std::make_unique<test::TestAppListControllerDelegate>();
    builder_ =
        std::make_unique<AppServicePromiseAppModelBuilder>(controller_.get());
    scoped_callback_ = std::make_unique<
        AppServicePromiseAppModelBuilder::ScopedAppPositionInitCallbackForTest>(
        builder_.get(),
        base::BindRepeating(
            &AppServicePromiseAppModelBuilderTest::InitAppPosition,
            weak_ptr_factory_.GetWeakPtr()));
    builder_->Initialize(nullptr, profile(), model_updater_.get());
    cache_ = apps::AppServiceProxyFactory::GetForProfile(profile())
                 ->PromiseAppRegistryCache();
  }

  void InitAppPosition(ChromeAppListItem* item) {
    if (!last_position_.IsValid()) {
      last_position_ = syncer::StringOrdinal::CreateInitialOrdinal();
    } else {
      last_position_ = last_position_.CreateAfter();
    }
    item->SetChromePosition(last_position_);
  }

  void RegisterTestApps() {
    // Register two promise apps in the promise app registry cache.
    apps::PromiseAppPtr promise_app_1 = std::make_unique<apps::PromiseApp>(
        apps::PackageId(apps::PackageType::kArc, "test1"));
    promise_app_1->should_show = true;
    cache()->OnPromiseApp(std::move(promise_app_1));

    apps::PromiseAppPtr promise_app_2 = std::make_unique<apps::PromiseApp>(
        apps::PackageId(apps::PackageType::kArc, "test2"));
    promise_app_2->should_show = true;
    cache()->OnPromiseApp(std::move(promise_app_2));
  }

  void BuildModelTest(bool guest_mode) {
    CreateBuilder(guest_mode);
    EXPECT_EQ(model_updater()->ItemCount(), 0u);

    RegisterTestApps();

    // Confirm there are 2 launcher promise app items.
    EXPECT_EQ(model_updater()->ItemCount(), 2u);
    EXPECT_EQ(model_updater()->ItemAtForTest(0)->id(), "android:test1");
    EXPECT_EQ(model_updater()->ItemAtForTest(1)->id(), "android:test2");
  }

  AppListModelUpdater* model_updater() { return model_updater_.get(); }

  apps::PromiseAppRegistryCache* cache() { return cache_; }

 private:
  apps::AppServiceTest app_service_test_;
  std::unique_ptr<
      AppServicePromiseAppModelBuilder::ScopedAppPositionInitCallbackForTest>
      scoped_callback_;
  std::unique_ptr<AppServicePromiseAppModelBuilder> builder_;
  std::unique_ptr<test::TestAppListControllerDelegate> controller_;
  std::unique_ptr<FakeAppListModelUpdater> model_updater_;
  display::test::TestScreen test_screen_;
  std::unique_ptr<Profile> profile_;
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<apps::PromiseAppRegistryCache> cache_;
  syncer::StringOrdinal last_position_;
  base::WeakPtrFactory<AppServicePromiseAppModelBuilderTest> weak_ptr_factory_{
      this};
};

TEST_F(AppServicePromiseAppModelBuilderTest, BuildModel) {
  BuildModelTest(/*guest_mode=*/true);
}

TEST_F(AppServicePromiseAppModelBuilderTest, BuildModelGuestMode) {
  BuildModelTest(/*guest_mode=*/false);
}
