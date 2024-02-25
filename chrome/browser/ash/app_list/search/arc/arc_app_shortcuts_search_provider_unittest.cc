// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/arc/arc_app_shortcuts_search_provider.h"

#include <memory>
#include <string>
#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/app_list/app_list_test_util.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ash/app_list/search/test/test_search_controller.h"
#include "chrome/browser/ash/app_list/test/test_app_list_controller_delegate.h"
#include "chrome/browser/ash/arc/icon_decode_request.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list::test {

namespace {
constexpr char kFakeAppPackageName[] = "FakeAppPackageName";
}  // namespace

class ArcAppShortcutsSearchProviderTest
    : public AppListTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  ArcAppShortcutsSearchProviderTest(const ArcAppShortcutsSearchProviderTest&) =
      delete;
  ArcAppShortcutsSearchProviderTest& operator=(
      const ArcAppShortcutsSearchProviderTest&) = delete;

 protected:
  ArcAppShortcutsSearchProviderTest() = default;
  ~ArcAppShortcutsSearchProviderTest() override = default;

  // AppListTestBase:
  void SetUp() override {
    AppListTestBase::SetUp();
    arc_test_.SetUp(profile());
    controller_ = std::make_unique<::test::TestAppListControllerDelegate>();
  }

  void TearDown() override {
    controller_.reset();
    arc_test_.TearDown();
    AppListTestBase::TearDown();
  }

  arc::mojom::AppInfoPtr CreateAppInfo(const std::string& name,
                                       const std::string& activity,
                                       const std::string& package_name) {
    auto appinfo = arc::mojom::AppInfo::New();
    appinfo->name = name;
    appinfo->package_name = package_name;
    appinfo->activity = activity;
    return appinfo;
  }

  std::string AddArcAppAndShortcut(const arc::mojom::AppInfo& app_info,
                                   bool launchable) {
    ArcAppListPrefs* const prefs = arc_test_.arc_app_list_prefs();

    std::optional<uint64_t> app_size_in_bytes;
    std::optional<uint64_t> data_size_in_bytes;

    if (!app_info.app_storage.is_null()) {
      app_size_in_bytes = app_info.app_storage->app_size_in_bytes;
      data_size_in_bytes = app_info.app_storage->data_size_in_bytes;
    }

    // Adding app to the prefs, and check that the app is accessible by id.
    prefs->AddAppAndShortcut(
        app_info.name, app_info.package_name, app_info.activity,
        std::string() /* intent_uri */, std::string() /* icon_resource_id */,
        app_info.version_name, false /* sticky */,
        true /* notifications_enabled */, true /* app_ready */,
        false /* suspended */, false /* shortcut */, launchable,
        app_info.need_fixup, ArcAppListPrefs::WindowLayout(), app_size_in_bytes,
        data_size_in_bytes, app_info.app_category);
    const std::string app_id =
        ArcAppListPrefs::GetAppId(app_info.package_name, app_info.activity);
    EXPECT_TRUE(prefs->GetApp(app_id));
    return app_id;
  }

  base::ScopedTempDir temp_dir_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<::test::TestAppListControllerDelegate> controller_;
  ArcAppTest arc_test_;
};

TEST_P(ArcAppShortcutsSearchProviderTest, Basic) {
  const bool launchable = GetParam();

  const std::string app_id = AddArcAppAndShortcut(
      *CreateAppInfo("FakeName", "FakeActivity", kFakeAppPackageName),
      launchable);

  const size_t kMaxResults = launchable ? 4 : 0;
  constexpr char16_t kQuery[] = u"shortlabel";

  TestSearchController search_controller;
  auto provider = std::make_unique<ArcAppShortcutsSearchProvider>(
      kMaxResults, profile(), controller_.get());
  search_controller.AddProvider(std::move(provider));
  EXPECT_TRUE(search_controller.last_results().empty());
  arc::IconDecodeRequest::DisableSafeDecodingForTesting();

  search_controller.StartSearch(kQuery);
  const auto& results = search_controller.last_results();
  EXPECT_EQ(kMaxResults, results.size());
  // Verify search results.
  for (size_t i = 0; i < results.size(); ++i) {
    EXPECT_EQ(base::StringPrintf("ShortLabel %zu", i),
              base::UTF16ToUTF8(results[i]->title()));
    EXPECT_EQ(ash::SearchResultDisplayType::kList, results[i]->display_type());
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         ArcAppShortcutsSearchProviderTest,
                         testing::Bool());

}  // namespace app_list::test
