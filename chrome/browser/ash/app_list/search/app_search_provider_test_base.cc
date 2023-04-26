// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/app_search_provider_test_base.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/components/arc/mojom/app.mojom.h"
#include "ash/components/arc/test/fake_app_instance.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "chrome/browser/ash/app_list/search/app_search_data_source.h"
#include "chrome/browser/ash/app_list/search/app_search_provider.h"
#include "chrome/browser/ash/app_list/search/app_zero_state_provider.h"
#include "chrome/browser/ash/app_list/search/search_provider.h"
#include "chrome/browser/ash/app_list/search/test/test_search_controller.h"
#include "chrome/browser/ash/app_list/test/test_app_list_controller_delegate.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync/model/string_ordinal.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::mojom::ManifestLocation;

namespace app_list {

AppSearchProviderTestBase::AppSearchProviderTestBase(bool zero_state_provider)
    : zero_state_provider_(zero_state_provider) {}

AppSearchProviderTestBase::~AppSearchProviderTestBase() = default;

void AppSearchProviderTestBase::SetUp() {
  AppListTestBase::SetUp();

  controller_ = std::make_unique<::test::TestAppListControllerDelegate>();
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
}

void AppSearchProviderTestBase::InitializeSearchProvider() {
  search_controller_ = std::make_unique<TestSearchController>();
  data_source_ =
      std::make_unique<AppSearchDataSource>(profile_.get(), nullptr, &clock_);

  std::unique_ptr<SearchProvider> app_search;
  if (zero_state_provider_) {
    app_search = std::make_unique<AppZeroStateProvider>(data_source_.get());
  } else {
    app_search = std::make_unique<AppSearchProvider>(data_source_.get());
  }

  app_search_ = app_search.get();

  search_controller_->AddProvider(std::move(app_search));
}

std::string AppSearchProviderTestBase::RunQuery(const std::string& query) {
  EXPECT_FALSE(query.empty());
  search_controller_->StartSearch(base::UTF8ToUTF16(query));
  return GetSortedResultsString();
}

std::string AppSearchProviderTestBase::RunZeroStateSearch() {
  search_controller_->StartZeroState(base::DoNothing(), base::TimeDelta());
  return GetSortedResultsString();
}

void AppSearchProviderTestBase::ClearSearch() {
  search_controller_->ClearSearch();
}

std::string AppSearchProviderTestBase::GetSortedResultsString() {
  // Sort results by relevance.
  std::vector<ChromeSearchResult*> sorted_results;
  for (const auto& result : search_controller_->last_results())
    sorted_results.emplace_back(result.get());
  std::sort(
      sorted_results.begin(), sorted_results.end(),
      [](const ChromeSearchResult* result1, const ChromeSearchResult* result2) {
        return result1->relevance() > result2->relevance();
      });

  std::string result_str;
  for (auto* result : sorted_results) {
    if (!result_str.empty())
      result_str += ',';
    result_str += base::UTF16ToUTF8(result->title());
  }
  return result_str;
}

std::vector<ChromeSearchResult*> AppSearchProviderTestBase::GetLastResults() {
  std::vector<ChromeSearchResult*> sorted_results;
  for (const auto& result : search_controller_->last_results())
    sorted_results.emplace_back(result.get());
  return sorted_results;
}

std::string AppSearchProviderTestBase::AddArcApp(const std::string& name,
                                                 const std::string& package,
                                                 const std::string& activity,
                                                 bool sticky) {
  arc::mojom::AppInfo app_info;
  app_info.name = name;
  app_info.package_name = package;
  app_info.activity = activity;
  app_info.sticky = sticky;
  app_info.notifications_enabled = false;
  arc_test_.app_instance()->SendAppAdded(app_info);
  return ArcAppListPrefs::GetAppId(package, activity);
}

void AppSearchProviderTestBase::AddExtension(const std::string& id,
                                             const std::string& name,
                                             ManifestLocation location,
                                             int init_from_value_flags,
                                             bool display_in_launcher) {
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder()
          .SetManifest(
              base::Value::Dict()
                  .Set("name", name)
                  .Set("version", "0.1")
                  .Set("app", base::Value::Dict().Set(
                                  "urls", base::Value::List().Append(
                                              "http://localhost/extensions/"
                                              "hosted_app/main.html")))
                  .Set("launch", base::Value::Dict().Set(
                                     "urls", base::Value::List().Append(
                                                 "http://localhost/extensions/"
                                                 "hosted_app/main.html")))
                  .Set("display_in_launcher", display_in_launcher))
          .SetLocation(location)
          .AddFlags(init_from_value_flags)
          .SetID(id)
          .Build();

  const syncer::StringOrdinal& page_ordinal =
      syncer::StringOrdinal::CreateInitialOrdinal();

  service()->OnExtensionInstalled(extension.get(), page_ordinal,
                                  extensions::kInstallFlagNone);
}

void AppSearchProviderTestBase::CallViewClosing() {
  app_search_->StopZeroState();
}

void AppSearchProviderTestBase::WaitTimeUpdated() {
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(1));
  run_loop.Run();
}

}  // namespace app_list
