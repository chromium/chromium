// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/app_service/app_service_shortcut_model_builder.h"

#include "ash/public/cpp/app_list/app_list_types.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/ash/app_list/app_list_test_util.h"
#include "chrome/browser/ash/app_list/chrome_app_list_item.h"
#include "chrome/browser/ash/app_list/internal_app/internal_app_metadata.h"
#include "chrome/browser/ash/app_list/test/fake_app_list_model_updater.h"
#include "chrome/browser/ash/app_list/test/test_app_list_controller_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut_registry_cache.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/test/test_screen.h"

class AppServiceShortcutModelBuilderTest
    : public app_list::AppListTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  AppServiceShortcutModelBuilderTest() = default;
  AppServiceShortcutModelBuilderTest(
      const AppServiceShortcutModelBuilderTest&) = delete;
  AppServiceShortcutModelBuilderTest& operator=(
      const AppServiceShortcutModelBuilderTest&) = delete;

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
    scoped_feature_list_.InitAndEnableFeature(
        chromeos::features::kCrosWebAppShortcutUiUpdate);
    testing_profile()->SetGuestSession(guest_mode);
    app_service_test_.SetUp(profile());
    model_updater_ = std::make_unique<FakeAppListModelUpdater>(
        /*profile=*/nullptr, /*reorder_delegate=*/nullptr);
    controller_ = std::make_unique<test::TestAppListControllerDelegate>();
    builder_ =
        std::make_unique<AppServiceShortcutModelBuilder>(controller_.get());
    scoped_callback_ = std::make_unique<
        AppServiceShortcutModelBuilder::ScopedAppPositionInitCallbackForTest>(
        builder_.get(),
        base::BindRepeating(
            &AppServiceShortcutModelBuilderTest::InitAppPosition,
            weak_ptr_factory_.GetWeakPtr()));
    builder_->Initialize(nullptr, profile(), model_updater_.get());
    cache_ = apps::AppServiceProxyFactory::GetForProfile(profile())
                 ->ShortcutRegistryCache();

    // Initial shortcuts in the system
    RegisterTestShortcut("host_app_id_1", "local_id_1", "Test 1");
    RegisterTestShortcut("host_app_id_1", "local_id_2", "Test 2");
  }

  void InitAppPosition(ChromeAppListItem* item) {
    if (!last_position_.IsValid()) {
      last_position_ = syncer::StringOrdinal::CreateInitialOrdinal();
    } else {
      last_position_ = last_position_.CreateAfter();
    }
    item->SetChromePosition(last_position_);
  }

  void RegisterTestShortcut(const std::string& host_app_id,
                            const std::string& local_id,
                            const std::string& name) {
    apps::ShortcutPtr shortcut =
        std::make_unique<apps::Shortcut>(host_app_id, local_id);
    shortcut->name = name;
    shortcut->shortcut_source = apps::ShortcutSource::kUser;
    cache()->UpdateShortcut(std::move(shortcut));
  }

  AppListModelUpdater* model_updater() { return model_updater_.get(); }

  apps::ShortcutRegistryCache* cache() { return cache_; }

 private:
  apps::AppServiceTest app_service_test_;
  std::unique_ptr<
      AppServiceShortcutModelBuilder::ScopedAppPositionInitCallbackForTest>
      scoped_callback_;
  std::unique_ptr<AppServiceShortcutModelBuilder> builder_;
  std::unique_ptr<test::TestAppListControllerDelegate> controller_;
  std::unique_ptr<FakeAppListModelUpdater> model_updater_;
  display::test::TestScreen test_screen_;
  std::unique_ptr<Profile> profile_;
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<apps::ShortcutRegistryCache> cache_;
  syncer::StringOrdinal last_position_;
  base::WeakPtrFactory<AppServiceShortcutModelBuilderTest> weak_ptr_factory_{
      this};
};

TEST_P(AppServiceShortcutModelBuilderTest, BuildModel) {
  CreateBuilder(GetParam());
  // Confirm there are 2 launcher shortcut items.
  EXPECT_EQ(model_updater()->ItemCount(), 2u);
  EXPECT_EQ(model_updater()->ItemAtForTest(0)->id(),
            apps::GenerateShortcutId("host_app_id_1", "local_id_1").value());
  EXPECT_EQ(model_updater()->ItemAtForTest(0)->name(), "Test 1");
  EXPECT_EQ(model_updater()->ItemAtForTest(1)->id(),
            apps::GenerateShortcutId("host_app_id_1", "local_id_2").value());
  EXPECT_EQ(model_updater()->ItemAtForTest(1)->name(), "Test 2");

  RegisterTestShortcut("host_app_id_2", "local_id_1", "Test 3");
  RegisterTestShortcut("host_app_id_2", "local_id_2", "Test 4");

  // Confirm there are 4 launcher shortcut items.
  EXPECT_EQ(model_updater()->ItemCount(), 4u);
  EXPECT_EQ(model_updater()->ItemAtForTest(2)->id(),
            apps::GenerateShortcutId("host_app_id_2", "local_id_1").value());
  EXPECT_EQ(model_updater()->ItemAtForTest(2)->name(), "Test 3");
  EXPECT_EQ(model_updater()->ItemAtForTest(3)->id(),
            apps::GenerateShortcutId("host_app_id_2", "local_id_2").value());
  EXPECT_EQ(model_updater()->ItemAtForTest(3)->name(), "Test 4");
}

TEST_P(AppServiceShortcutModelBuilderTest, RemoveShortcut) {
  CreateBuilder(GetParam());
  // Confirm there are 2 launcher shortcut items.
  EXPECT_EQ(model_updater()->ItemCount(), 2u);
  EXPECT_EQ(model_updater()->ItemAtForTest(0)->id(),
            apps::GenerateShortcutId("host_app_id_1", "local_id_1").value());
  EXPECT_EQ(model_updater()->ItemAtForTest(0)->name(), "Test 1");
  EXPECT_EQ(model_updater()->ItemAtForTest(1)->id(),
            apps::GenerateShortcutId("host_app_id_1", "local_id_2").value());
  EXPECT_EQ(model_updater()->ItemAtForTest(1)->name(), "Test 2");

  cache()->RemoveShortcut(
      apps::ShortcutId(model_updater()->ItemAtForTest(0)->id()));
  EXPECT_EQ(model_updater()->ItemCount(), 1u);

  cache()->RemoveShortcut(
      apps::ShortcutId(model_updater()->ItemAtForTest(0)->id()));
  EXPECT_EQ(model_updater()->ItemCount(), 0u);
}

INSTANTIATE_TEST_SUITE_P(/*no prefix*/,
                         AppServiceShortcutModelBuilderTest,
                         testing::Values(true, false));
