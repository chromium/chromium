// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/extensions_permissions_tracker.h"

#include "base/stl_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/login/login_state/scoped_test_public_session_login_state.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/value_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;

namespace extensions {

namespace {
constexpr char kExtensionId1[] = "id1";
constexpr char kExtensionId2[] = "id2";
constexpr char kExtensionId3[] = "id3";
constexpr char kExtensionUrl1[] = "url1";
constexpr char kExtensionUrl2[] = "url2";
constexpr char kExtensionUrl3[] = "url3";
const char* const kSafePermissionsSet1[] = {"accessibilityFeatures.modify",
                                            "accessibilityFeatures.read"};

const char* const kSafePermissionsSet2[] = {"background", "alarms"};

const char* const kUnsafePermissionsSet1[] = {"debugger", "history", "input"};

const char* const kUnsafePermissionsSet2[] = {"topSites", "ttsEngine",
                                              "webNavigation"};
}  // namespace

class MockExtensionsPermissionsTracker : public ExtensionsPermissionsTracker {
 public:
  MockExtensionsPermissionsTracker(ExtensionRegistry* registry,
                                   content::BrowserContext* browser_context)
      : ExtensionsPermissionsTracker(registry, browser_context) {
    safe_permissions_.insert(
        kSafePermissionsSet1,
        kSafePermissionsSet1 + base::size(kSafePermissionsSet1));
    safe_permissions_.insert(
        kSafePermissionsSet2,
        kSafePermissionsSet2 + base::size(kSafePermissionsSet2));
  }

  // ExtensionsPermissionsTracker:
  bool IsSafePerms(const PermissionsData* perms_data) const override {
    std::set<std::string> perms_strings =
        perms_data->active_permissions().GetAPIsAsStrings();
    for (const auto& perm : perms_strings) {
      if (safe_permissions_.find(perm) == safe_permissions_.end())
        return false;
    }
    return true;
  }

 private:
  std::set<std::string> safe_permissions_;
};

class ExtensionsPermissionsTrackerTest : public testing::Test {
 public:
  ExtensionsPermissionsTrackerTest()
      : prefs_(profile_.GetTestingPrefService()),
        registry_(ExtensionRegistry::Get(&profile_)),
        testing_local_state_(TestingBrowserProcess::GetGlobal()) {}

  base::Value SetupForceList() {
    base::Value dict(base::Value::Type::DICTIONARY);
    dict.SetKey(kExtensionId1, base::Value(kExtensionUrl1));
    dict.SetKey(kExtensionId2, base::Value(kExtensionUrl2));
    prefs_->SetManagedPref(pref_names::kInstallForceList,
                           base::Value::ToUniquePtrValue(dict.Clone()));
    return dict;
  }

  void SetupEmptyForceList() {
    std::unique_ptr<base::Value> dict =
        std::make_unique<base::DictionaryValue>();
    prefs_->SetManagedPref(pref_names::kInstallForceList, std::move(dict));
  }

  void CreateExtensionsPermissionsTracker() {
    permissions_tracker_ = std::make_unique<MockExtensionsPermissionsTracker>(
        registry_, &profile_);
  }

  void AddExtensionWithIdAndPermissions(
      const std::string& extension_id,
      const std::vector<std::string>& permissions) {
    auto extension = ExtensionBuilder(extension_id)
                         .SetID(extension_id)
                         .AddPermissions(permissions)
                         .Build();
    registry_->AddEnabled(extension);

    registry_->TriggerOnLoaded(extension.get());
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  sync_preferences::TestingPrefServiceSyncable* prefs_;
  ExtensionRegistry* registry_;
  ScopedTestingLocalState testing_local_state_;
  std::unique_ptr<MockExtensionsPermissionsTracker> permissions_tracker_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionsPermissionsTrackerTest);
};

TEST_F(ExtensionsPermissionsTrackerTest, EmptyForceList) {
  EXPECT_TRUE(testing_local_state_.Get()->GetBoolean(
      prefs::kManagedSessionUseFullLoginWarning));

  SetupEmptyForceList();
  CreateExtensionsPermissionsTracker();

  EXPECT_FALSE(testing_local_state_.Get()->GetBoolean(
      prefs::kManagedSessionUseFullLoginWarning));
}

TEST_F(ExtensionsPermissionsTrackerTest, SafeForceListInstalled) {
  EXPECT_TRUE(testing_local_state_.Get()->GetBoolean(
      prefs::kManagedSessionUseFullLoginWarning));

  SetupForceList();
  CreateExtensionsPermissionsTracker();

  std::vector<std::string> v1(
      kSafePermissionsSet1,
      kSafePermissionsSet1 + base::size(kSafePermissionsSet1));
  AddExtensionWithIdAndPermissions(kExtensionId1, v1);

  std::vector<std::string> v2(
      kSafePermissionsSet2,
      kSafePermissionsSet2 + base::size(kSafePermissionsSet2));
  AddExtensionWithIdAndPermissions(kExtensionId2, v2);

  EXPECT_FALSE(testing_local_state_.Get()->GetBoolean(
      prefs::kManagedSessionUseFullLoginWarning));
}

TEST_F(ExtensionsPermissionsTrackerTest, UnsafeForceListInstalled) {
  SetupForceList();
  CreateExtensionsPermissionsTracker();

  std::vector<std::string> v1(
      kUnsafePermissionsSet1,
      kUnsafePermissionsSet1 + base::size(kUnsafePermissionsSet1));

  std::vector<std::string> v2(
      kUnsafePermissionsSet2,
      kUnsafePermissionsSet2 + base::size(kUnsafePermissionsSet2));

  AddExtensionWithIdAndPermissions(kExtensionId1, v1);
  AddExtensionWithIdAndPermissions(kExtensionId2, v2);

  EXPECT_TRUE(testing_local_state_.Get()->GetBoolean(
      prefs::kManagedSessionUseFullLoginWarning));
}

TEST_F(ExtensionsPermissionsTrackerTest, MixedForceListInstalled) {
  SetupForceList();
  CreateExtensionsPermissionsTracker();

  std::vector<std::string> v1(
      kUnsafePermissionsSet1,
      kUnsafePermissionsSet1 + base::size(kUnsafePermissionsSet1));
  AddExtensionWithIdAndPermissions(kExtensionId1, v1);

  std::vector<std::string> v2(
      kSafePermissionsSet2,
      kSafePermissionsSet2 + base::size(kSafePermissionsSet2));
  AddExtensionWithIdAndPermissions(kExtensionId2, v2);

  EXPECT_TRUE(testing_local_state_.Get()->GetBoolean(
      prefs::kManagedSessionUseFullLoginWarning));
}

TEST_F(ExtensionsPermissionsTrackerTest, ForceListIncreased) {
  auto dict = SetupForceList();
  CreateExtensionsPermissionsTracker();

  std::vector<std::string> v1(
      kSafePermissionsSet1,
      kSafePermissionsSet1 + base::size(kSafePermissionsSet1));
  AddExtensionWithIdAndPermissions(kExtensionId1, v1);

  std::vector<std::string> v2(
      kSafePermissionsSet2,
      kSafePermissionsSet2 + base::size(kSafePermissionsSet2));
  AddExtensionWithIdAndPermissions(kExtensionId2, v2);

  EXPECT_FALSE(testing_local_state_.Get()->GetBoolean(
      prefs::kManagedSessionUseFullLoginWarning));

  dict.SetKey(kExtensionId3, base::Value(kExtensionUrl3));
  prefs_->SetManagedPref(pref_names::kInstallForceList,
                         base::Value::ToUniquePtrValue(std::move(dict)));

  std::vector<std::string> v3(
      kUnsafePermissionsSet1,
      kUnsafePermissionsSet1 + base::size(kUnsafePermissionsSet1));
  AddExtensionWithIdAndPermissions(kExtensionId3, v3);

  EXPECT_TRUE(testing_local_state_.Get()->GetBoolean(
      prefs::kManagedSessionUseFullLoginWarning));
}

TEST_F(ExtensionsPermissionsTrackerTest, ForceListDecreased) {
  auto dict = SetupForceList();
  CreateExtensionsPermissionsTracker();

  std::vector<std::string> v1(
      kUnsafePermissionsSet1,
      kUnsafePermissionsSet1 + base::size(kUnsafePermissionsSet1));
  AddExtensionWithIdAndPermissions(kExtensionId1, v1);

  std::vector<std::string> v2(
      kSafePermissionsSet2,
      kSafePermissionsSet2 + base::size(kSafePermissionsSet2));
  AddExtensionWithIdAndPermissions(kExtensionId2, v2);

  EXPECT_TRUE(testing_local_state_.Get()->GetBoolean(
      prefs::kManagedSessionUseFullLoginWarning));

  dict.RemoveKey(kExtensionId1);
  prefs_->SetManagedPref(pref_names::kInstallForceList,
                         base::Value::ToUniquePtrValue(std::move(dict)));
  EXPECT_FALSE(testing_local_state_.Get()->GetBoolean(
      prefs::kManagedSessionUseFullLoginWarning));
}

TEST_F(ExtensionsPermissionsTrackerTest, SafePendingExtensions) {
  auto dict = SetupForceList();
  CreateExtensionsPermissionsTracker();

  std::vector<std::string> v1(
      kSafePermissionsSet1,
      kSafePermissionsSet1 + base::size(kSafePermissionsSet1));
  AddExtensionWithIdAndPermissions(kExtensionId1, v1);

  EXPECT_TRUE(testing_local_state_.Get()->GetBoolean(
      prefs::kManagedSessionUseFullLoginWarning));

  std::vector<std::string> v2(
      kSafePermissionsSet2,
      kSafePermissionsSet2 + base::size(kSafePermissionsSet2));
  AddExtensionWithIdAndPermissions(kExtensionId2, v2);

  EXPECT_FALSE(testing_local_state_.Get()->GetBoolean(
      prefs::kManagedSessionUseFullLoginWarning));
}

TEST_F(ExtensionsPermissionsTrackerTest, UnsafePendingExtensions) {
  auto dict = SetupForceList();
  CreateExtensionsPermissionsTracker();

  std::vector<std::string> v1(
      kSafePermissionsSet1,
      kSafePermissionsSet1 + base::size(kSafePermissionsSet1));
  AddExtensionWithIdAndPermissions(kExtensionId1, v1);

  EXPECT_TRUE(testing_local_state_.Get()->GetBoolean(
      prefs::kManagedSessionUseFullLoginWarning));

  std::vector<std::string> v2(
      kUnsafePermissionsSet1,
      kUnsafePermissionsSet1 + base::size(kUnsafePermissionsSet1));
  AddExtensionWithIdAndPermissions(kExtensionId2, v2);

  EXPECT_TRUE(testing_local_state_.Get()->GetBoolean(
      prefs::kManagedSessionUseFullLoginWarning));
}

TEST_F(ExtensionsPermissionsTrackerTest, UnsafeForceListChanged) {
  auto dict = SetupForceList();
  CreateExtensionsPermissionsTracker();

  std::vector<std::string> v1(
      kSafePermissionsSet1,
      kSafePermissionsSet1 + base::size(kSafePermissionsSet1));
  AddExtensionWithIdAndPermissions(kExtensionId1, v1);

  std::vector<std::string> v2(
      kUnsafePermissionsSet1,
      kUnsafePermissionsSet1 + base::size(kUnsafePermissionsSet1));
  AddExtensionWithIdAndPermissions(kExtensionId2, v2);

  EXPECT_TRUE(testing_local_state_.Get()->GetBoolean(
      prefs::kManagedSessionUseFullLoginWarning));

  dict.RemoveKey(kExtensionId1);
  prefs_->SetManagedPref(pref_names::kInstallForceList,
                         base::Value::ToUniquePtrValue(dict.Clone()));

  EXPECT_TRUE(testing_local_state_.Get()->GetBoolean(
      prefs::kManagedSessionUseFullLoginWarning));

  dict.RemoveKey(kExtensionId2);
  prefs_->SetManagedPref(pref_names::kInstallForceList,
                         base::Value::ToUniquePtrValue(dict.Clone()));

  EXPECT_FALSE(testing_local_state_.Get()->GetBoolean(
      prefs::kManagedSessionUseFullLoginWarning));
}

TEST_F(ExtensionsPermissionsTrackerTest, OtherExtensionsLoaded) {
  auto dict = SetupForceList();
  CreateExtensionsPermissionsTracker();

  std::vector<std::string> v1(
      kSafePermissionsSet1,
      kSafePermissionsSet1 + base::size(kSafePermissionsSet1));
  AddExtensionWithIdAndPermissions(kExtensionId1, v1);

  std::vector<std::string> v2(
      kSafePermissionsSet1,
      kSafePermissionsSet1 + base::size(kSafePermissionsSet1));
  AddExtensionWithIdAndPermissions(kExtensionId2, v2);

  std::vector<std::string> v3(
      kUnsafePermissionsSet1,
      kUnsafePermissionsSet1 + base::size(kUnsafePermissionsSet1));
  AddExtensionWithIdAndPermissions(kExtensionId3, v3);

  EXPECT_FALSE(testing_local_state_.Get()->GetBoolean(
      prefs::kManagedSessionUseFullLoginWarning));
}

}  // namespace extensions
