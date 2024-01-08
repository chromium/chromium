// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/chrome_app_list_item.h"

#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/public/cpp/test/app_list_test_api.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ash/app_list/app_list_client_impl.h"
#include "chrome/browser/ash/app_list/app_list_model_updater.h"
#include "chrome/browser/ash/app_list/test/chrome_app_list_test_support.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/gfx/image/image_skia.h"

namespace {

// An ImageSkiaSource that counts the number of times it is asked for image
// representations.
class TestChromeAppListItem : public ChromeAppListItem {
 public:
  TestChromeAppListItem(Profile* profile,
                        const std::string& app_id,
                        AppListModelUpdater* model_updater)
      : ChromeAppListItem(profile, app_id, model_updater) {}
  ~TestChromeAppListItem() override = default;

  // ChromeAppListItem:
  void LoadIcon() override { ++count_; }

  int GetLoadIconCountAndReset() {
    int current_count = count_;
    count_ = 0;
    return current_count;
  }

 private:
  int count_ = 0;
};

}  // namespace

class ChromeAppListItemTest : public InProcessBrowserTest {
 public:
  ChromeAppListItemTest() = default;
  ~ChromeAppListItemTest() override = default;

  // InProcessBrowserTest
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    client_ = AppListClientImpl::GetInstance();
    ASSERT_TRUE(client_);
    client_->UpdateProfile();

    model_updater_ = test::GetModelUpdater(client_);
  }

  void ShowLauncherAppsGrid() {
    EXPECT_FALSE(client_->GetAppListWindow());
    ash::AcceleratorController::Get()->PerformActionIfEnabled(
        ash::AcceleratorAction::kToggleAppList, {});
    ash::AppListTestApi().WaitForBubbleWindow(
        /*wait_for_opening_animation=*/false);
    EXPECT_TRUE(client_->GetAppListWindow());
  }

  Profile* profile() { return ProfileManager::GetActiveUserProfile(); }

 protected:
  raw_ptr<AppListClientImpl, DanglingUntriaged> client_ = nullptr;
  raw_ptr<AppListModelUpdater, DanglingUntriaged> model_updater_ = nullptr;
};

// Tests that app icon load is deferred until UI is shown.
IN_PROC_BROWSER_TEST_F(ChromeAppListItemTest, IconLoadAfterShowingUI) {
  constexpr char kAppId[] = "FakeAppId";
  constexpr char kAppName[] = "FakeApp";
  auto app_item_ptr = std::make_unique<TestChromeAppListItem>(profile(), kAppId,
                                                              model_updater_);
  TestChromeAppListItem* app_item = app_item_ptr.get();
  model_updater_->AddItem(std::move(app_item_ptr));
  model_updater_->SetItemName(app_item->id(), kAppName);
  // No icon loading on creating an app item without UI.
  EXPECT_EQ(0, app_item->GetLoadIconCountAndReset());

  ShowLauncherAppsGrid();

  // Icon loading is triggered after showing the launcher.
  EXPECT_GT(app_item->GetLoadIconCountAndReset(), 0);
}

// Tests that icon loading is synchronous when UI is visible.
IN_PROC_BROWSER_TEST_F(ChromeAppListItemTest, IconLoadWithUI) {
  ShowLauncherAppsGrid();

  constexpr char kAppId[] = "FakeAppId";
  constexpr char kAppName[] = "FakeApp";
  auto app_item_ptr = std::make_unique<TestChromeAppListItem>(profile(), kAppId,
                                                              model_updater_);
  TestChromeAppListItem* app_item = app_item_ptr.get();
  model_updater_->AddItem(std::move(app_item_ptr));
  model_updater_->SetItemName(app_item->id(), kAppName);

  // Icon load happens synchronously when UI is visible.
  EXPECT_GT(app_item->GetLoadIconCountAndReset(), 0);
}

// Combination of IconLoadAfterShowingUI and IconLoadWithUI but for apps inside
// a folder.
IN_PROC_BROWSER_TEST_F(ChromeAppListItemTest, FolderIconLoad) {
  std::vector<TestChromeAppListItem*> apps;

  // A folder should exist before adding children.
  constexpr char kFakeFolderId[] = "FakeFolder";
  auto folder_item_ptr = std::make_unique<TestChromeAppListItem>(
      profile(), kFakeFolderId, model_updater_);
  folder_item_ptr->SetChromeIsFolder(true);
  folder_item_ptr->SetChromePosition(
      syncer::StringOrdinal::CreateInitialOrdinal());
  model_updater_->AddItem(std::move(folder_item_ptr));

  for (int i = 0; i < 2; ++i) {
    std::string app_id = base::StringPrintf("FakeAppId_%d", i);
    std::string app_name = base::StringPrintf("FakeApp_%d", i);
    auto app_item_ptr = std::make_unique<TestChromeAppListItem>(
        profile(), app_id, model_updater_);
    TestChromeAppListItem* app_item = app_item_ptr.get();
    apps.push_back(app_item);

    model_updater_->AddAppItemToFolder(std::move(app_item_ptr), kFakeFolderId,
                                       /*add_from_local=*/true);
    model_updater_->SetItemName(app_item->id(), app_name);

    // No icon loading on creating an app item.
    EXPECT_EQ(0, app_item->GetLoadIconCountAndReset());
  }

  ShowLauncherAppsGrid();

  // Icon loading is triggered after showing the launcher.
  for (auto* app_item : apps) {
    SCOPED_TRACE(testing::Message() << "app_id=" << app_item->id());

    EXPECT_GT(app_item->GetLoadIconCountAndReset(), 0);
  }

  // Adds a new item to folder while UI is visible.
  auto app_item_ptr = std::make_unique<TestChromeAppListItem>(
      profile(), "AnotherAppId", model_updater_);
  TestChromeAppListItem* app_item = app_item_ptr.get();
  model_updater_->AddAppItemToFolder(std::move(app_item_ptr), kFakeFolderId,
                                     /*add_from_local=*/true);
  model_updater_->SetItemName(app_item->id(), "AnotherApp");

  // Icon load happens synchronously when UI is visible.
  EXPECT_GT(app_item->GetLoadIconCountAndReset(), 0);
}
