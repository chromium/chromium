// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/counters/hosted_apps_counter.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/uuid.h"
#include "base/values.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/crx_file/id_util.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class HostedAppsCounterTest : public testing::Test {
 public:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    extension_registry_ = extensions::ExtensionRegistry::Get(profile_.get());

    SetHostedAppsDeletionPref(true);
    SetDeletionPeriodPref(browsing_data::TimePeriod::ALL_TIME);
  }

  // Adding and removing apps and extensions. ----------------------------------

  std::string AddExtension() {
    return AddItem(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                   /*app_manifest=*/std::nullopt);
  }

  std::string AddPackagedApp() {
    return AddItem(
        base::Uuid::GenerateRandomV4().AsLowercaseString(),
        base::Value::Dict().Set(
            "launch", base::Value::Dict().Set("local_path", "index.html")));
  }

  std::string AddHostedApp() {
    return AddHostedAppWithName(
        base::Uuid::GenerateRandomV4().AsLowercaseString());
  }

  std::string AddHostedAppWithName(const std::string& name) {
    return AddItem(
        name,
        base::Value::Dict()
            .Set("urls", base::Value::List().Append("https://example.com"))
            .Set("launch",
                 base::Value::Dict().Set("web_url", "https://example.com")));
  }

  std::string AddItem(const std::string& name,
                      std::optional<base::Value::Dict> app_manifest) {
    auto manifest_builder = base::Value::Dict()
                                .Set("manifest_version", 2)
                                .Set("name", name)
                                .Set("version", "1");

    if (app_manifest) {
      manifest_builder.Set("app", std::move(*app_manifest));
    }

    scoped_refptr<const extensions::Extension> item =
        extensions::ExtensionBuilder()
            .SetManifest(std::move(manifest_builder))
            .SetID(crx_file::id_util::GenerateId(name))
            .Build();

    extension_registry_->AddEnabled(item.get());
    return item->id();
  }

  void RemoveItem(const std::string& id) {
    extension_registry_->RemoveEnabled(id);
  }

  // Setting preferences. ------------------------------------------------------

  void SetHostedAppsDeletionPref(bool value) {
    GetProfile()->GetPrefs()->SetBoolean(
        browsing_data::prefs::kDeleteHostedAppsData, value);
  }

  void SetDeletionPeriodPref(browsing_data::TimePeriod period) {
    GetProfile()->GetPrefs()->SetInteger(
        browsing_data::prefs::kDeleteTimePeriod, static_cast<int>(period));
  }

  // Retrieving counter results. -----------------------------------------------

  browsing_data::BrowsingDataCounter::ResultInt GetNumHostedApps() {
    DCHECK(finished_);
    return num_apps_;
  }

  const std::vector<std::string>& GetExamples() {
    DCHECK(finished_);
    return examples_;
  }

  void Callback(
      std::unique_ptr<browsing_data::BrowsingDataCounter::Result> result) {
    finished_ = result->Finished();

    if (finished_) {
      HostedAppsCounter::HostedAppsResult* hosted_apps_result =
          static_cast<HostedAppsCounter::HostedAppsResult*>(result.get());

      num_apps_ = hosted_apps_result->Value();
      examples_ = hosted_apps_result->examples();
    }
  }

  // Miscellaneous. ------------------------------------------------------------

  Profile* GetProfile() {
    return profile_.get();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<extensions::ExtensionRegistry> extension_registry_;

  bool finished_;
  browsing_data::BrowsingDataCounter::ResultInt num_apps_;
  std::vector<std::string> examples_;
};

// Tests that we count the total number of hosted apps correctly.
TEST_F(HostedAppsCounterTest, Count) {
  Profile* profile = GetProfile();
  HostedAppsCounter counter(profile);
  counter.Init(profile->GetPrefs(),
               browsing_data::ClearBrowsingDataTab::ADVANCED,
               base::BindRepeating(&HostedAppsCounterTest::Callback,
                                   base::Unretained(this)));
  counter.Restart();
  EXPECT_EQ(0u, GetNumHostedApps());

  std::string first_app = AddHostedApp();
  AddHostedApp();
  std::string last_app = AddHostedApp();

  counter.Restart();
  EXPECT_EQ(3u, GetNumHostedApps());

  RemoveItem(last_app);
  RemoveItem(first_app);
  counter.Restart();
  EXPECT_EQ(1u, GetNumHostedApps());

  AddHostedApp();
  counter.Restart();
  EXPECT_EQ(2u, GetNumHostedApps());
}

// Tests that we only count hosted apps, not packaged apps or extensions.
TEST_F(HostedAppsCounterTest, OnlyHostedApps) {
  Profile* profile = GetProfile();
  HostedAppsCounter counter(profile);
  counter.Init(profile->GetPrefs(),
               browsing_data::ClearBrowsingDataTab::ADVANCED,
               base::BindRepeating(&HostedAppsCounterTest::Callback,
                                   base::Unretained(this)));

  AddHostedApp();  // 1
  AddExtension();
  AddPackagedApp();
  AddExtension();
  counter.Restart();
  EXPECT_EQ(1u, GetNumHostedApps());

  AddHostedApp();  // 2
  AddHostedApp();  // 3
  AddExtension();
  counter.Restart();
  EXPECT_EQ(3u, GetNumHostedApps());

  AddPackagedApp();
  AddExtension();
  counter.Restart();
  EXPECT_EQ(3u, GetNumHostedApps());

  AddHostedApp();  // 4
  AddPackagedApp();
  AddHostedApp();  // 5
  AddExtension();
  AddExtension();
  AddExtension();
  AddPackagedApp();
  counter.Restart();
  EXPECT_EQ(5u, GetNumHostedApps());
}

// Tests that the counter results contain names of the first two hosted apps
// in lexicographic ordering.
TEST_F(HostedAppsCounterTest, Examples) {
  Profile* profile = GetProfile();
  HostedAppsCounter counter(profile);
  counter.Init(profile->GetPrefs(),
               browsing_data::ClearBrowsingDataTab::ADVANCED,
               base::BindRepeating(&HostedAppsCounterTest::Callback,
                                   base::Unretained(this)));
  counter.Restart();
  EXPECT_EQ(0u, GetExamples().size());

  AddHostedAppWithName("App 1");
  counter.Restart();
  EXPECT_EQ(1u, GetExamples().size());
  EXPECT_EQ("App 1", GetExamples().front());

  AddHostedAppWithName("App 2");
  counter.Restart();
  EXPECT_EQ(2u, GetExamples().size());
  EXPECT_EQ("App 1", GetExamples().front());
  EXPECT_EQ("App 2", GetExamples().back());

  AddHostedAppWithName("App 3");
  counter.Restart();
  EXPECT_EQ(2u, GetExamples().size());
  EXPECT_EQ("App 1", GetExamples().front());
  EXPECT_EQ("App 2", GetExamples().back());
}

}  // namespace
