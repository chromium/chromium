// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/storage/policy_value_store.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_writer.h"
#include "base/memory/ref_counted.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/value_store/leveldb_value_store.h"
#include "components/value_store/value_store_test_suite.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/storage/backend_task_runner.h"
#include "extensions/browser/api/storage/settings_observer.h"
#include "extensions/browser/api/storage/storage_area_namespace.h"
#include "extensions/common/extension_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Mock;
using testing::NiceMock;
using value_store::ValueStore;
using value_store::ValueStoreTestSuite;

namespace extensions {

namespace {

const char kTestExtensionId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
const char kDatabaseUMAClientName[] = "Test";

std::string ValueToJson(const base::Value& changes) {
  std::string json;
  bool success = base::JSONWriter::Write(changes, &json);
  DCHECK(success);
  return json;
}

std::string ValueStoreChangeToJson(value_store::ValueStoreChangeList changes) {
  return ValueToJson(
      value_store::ValueStoreChange::ToValue(std::move(changes)));
}

class MockSettingsObserver {
 public:
  MOCK_METHOD4(
      OnSettingsChangedJSON,
      void(const ExtensionId& extension_id,
           StorageAreaNamespace storage_area,
           std::optional<api::storage::AccessLevel> session_access_level,
           const std::string& changes_json));

  void OnSettingsChanged(
      const ExtensionId& extension_id,
      StorageAreaNamespace storage_area,
      std::optional<api::storage::AccessLevel> session_access_level,
      base::Value changes) {
    OnSettingsChangedJSON(extension_id, storage_area, session_access_level,
                          ValueToJson(std::move(changes)));
  }
};

// Extends PolicyValueStore by overriding the mutating methods, so that the
// Get() base implementation can be tested with the ValueStoreTestSuite
// parameterized tests.
class MutablePolicyValueStore : public PolicyValueStore {
 public:
  explicit MutablePolicyValueStore(const base::FilePath& path)
      : PolicyValueStore(
            kTestExtensionId,
            SequenceBoundSettingsChangedCallback(base::DoNothing()),
            std::make_unique<value_store::LeveldbValueStore>(
                kDatabaseUMAClientName,
                path)) {}

  MutablePolicyValueStore(const MutablePolicyValueStore&) = delete;
  MutablePolicyValueStore& operator=(const MutablePolicyValueStore&) = delete;

  ~MutablePolicyValueStore() override = default;

  WriteResult Set(WriteOptions options,
                  const std::string& key,
                  const base::Value& value) override {
    return delegate()->Set(options, key, value);
  }

  WriteResult Set(WriteOptions options,
                  const base::Value::Dict& values) override {
    return delegate()->Set(options, values);
  }

  WriteResult Remove(const std::string& key) override {
    return delegate()->Remove(key);
  }

  WriteResult Remove(const std::vector<std::string>& keys) override {
    return delegate()->Remove(keys);
  }

  WriteResult Clear() override { return delegate()->Clear(); }
};

ValueStore* Param(const base::FilePath& file_path) {
  return new MutablePolicyValueStore(file_path);
}

}  // namespace

INSTANTIATE_TEST_SUITE_P(PolicyValueStoreTest,
                         ValueStoreTestSuite,
                         testing::Values(&Param));

class PolicyValueStoreTest : public testing::Test {
 public:
  PolicyValueStoreTest() = default;
  ~PolicyValueStoreTest() override {}

  void SetUp() override {
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    store_ = std::make_unique<PolicyValueStore>(
        kTestExtensionId,
        SequenceBoundSettingsChangedCallback(
            base::BindRepeating(&MockSettingsObserver::OnSettingsChanged,
                                base::Unretained(&observer_))),
        std::make_unique<value_store::LeveldbValueStore>(
            kDatabaseUMAClientName, scoped_temp_dir_.GetPath()));
  }

  void TearDown() override { store_.reset(); }

 protected:
  void SetCurrentPolicy(const policy::PolicyMap& policies) {
    GetBackendTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&PolicyValueStoreTest::SetCurrentPolicyOnBackendSequence,
                       base::Unretained(this), policies.Clone()));
    content::RunAllTasksUntilIdle();
  }

  void SetCurrentPolicyOnBackendSequence(const policy::PolicyMap& policies) {
    DCHECK(IsOnBackendSequence());
    store_->SetCurrentPolicy(policies);
  }

  base::ScopedTempDir scoped_temp_dir_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<PolicyValueStore> store_;
  NiceMock<MockSettingsObserver> observer_;
};

TEST_F(PolicyValueStoreTest, DontProvideRecommendedPolicies) {
  policy::PolicyMap policies;
  base::Value expected(123);
  policies.Set("must", policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
               expected.Clone(), nullptr);
  policies.Set("may", policy::POLICY_LEVEL_RECOMMENDED,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
               base::Value(456), nullptr);
  SetCurrentPolicy(policies);

  ValueStore::ReadResult result = store_->Get();
  ASSERT_TRUE(result.status().ok());
  EXPECT_EQ(1u, result.settings().size());
  base::Value* value = result.settings().Find("may");
  EXPECT_FALSE(value);
  value = result.settings().Find("must");
  ASSERT_TRUE(value);
  EXPECT_EQ(expected, *value);
}

TEST_F(PolicyValueStoreTest, ReadOnly) {
  ValueStore::WriteOptions options = ValueStore::DEFAULTS;

  base::Value string_value("value");
  EXPECT_FALSE(store_->Set(options, "key", string_value).status().ok());

  base::Value::Dict dict;
  dict.Set("key", "value");
  EXPECT_FALSE(store_->Set(options, dict).status().ok());

  EXPECT_FALSE(store_->Remove("key").status().ok());
  std::vector<std::string> keys;
  keys.push_back("key");
  EXPECT_FALSE(store_->Remove(keys).status().ok());
  EXPECT_FALSE(store_->Clear().status().ok());
}

TEST_F(PolicyValueStoreTest, NotifyOnChanges) {
  // Notify when setting the initial policy.
  const base::Value value("111");
  {
    value_store::ValueStoreChangeList changes;
    changes.push_back(
        value_store::ValueStoreChange("aaa", std::nullopt, value.Clone()));
    EXPECT_CALL(observer_,
                OnSettingsChangedJSON(
                    kTestExtensionId, StorageAreaNamespace::kManaged,
                    /*session_access_level=*/testing::Eq(std::nullopt),
                    ValueStoreChangeToJson(std::move(changes))));
  }

  policy::PolicyMap policies;
  policies.Set("aaa", policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, value.Clone(), nullptr);
  SetCurrentPolicy(policies);
  Mock::VerifyAndClearExpectations(&observer_);

  // Notify when new policies are added.
  {
    value_store::ValueStoreChangeList changes;
    changes.push_back(
        value_store::ValueStoreChange("bbb", std::nullopt, value.Clone()));
    EXPECT_CALL(observer_,
                OnSettingsChangedJSON(
                    kTestExtensionId, StorageAreaNamespace::kManaged,
                    /*session_access_level=*/testing::Eq(std::nullopt),
                    ValueStoreChangeToJson(std::move(changes))));
  }

  policies.Set("bbb", policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, value.Clone(), nullptr);
  SetCurrentPolicy(policies);
  Mock::VerifyAndClearExpectations(&observer_);

  // Notify when policies change.
  const base::Value new_value("222");
  {
    value_store::ValueStoreChangeList changes;
    changes.push_back(
        value_store::ValueStoreChange("bbb", value.Clone(), new_value.Clone()));
    EXPECT_CALL(observer_,
                OnSettingsChangedJSON(
                    kTestExtensionId, StorageAreaNamespace::kManaged,
                    /*session_access_level=*/testing::Eq(std::nullopt),
                    ValueStoreChangeToJson(std::move(changes))));
  }

  policies.Set("bbb", policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, new_value.Clone(), nullptr);
  SetCurrentPolicy(policies);
  Mock::VerifyAndClearExpectations(&observer_);

  // Notify when policies are removed.
  {
    value_store::ValueStoreChangeList changes;
    changes.push_back(
        value_store::ValueStoreChange("bbb", new_value.Clone(), std::nullopt));
    EXPECT_CALL(observer_,
                OnSettingsChangedJSON(
                    kTestExtensionId, StorageAreaNamespace::kManaged,

                    /*session_access_level=*/testing::Eq(std::nullopt),
                    ValueStoreChangeToJson(std::move(changes))));
  }

  policies.Erase("bbb");
  SetCurrentPolicy(policies);
  Mock::VerifyAndClearExpectations(&observer_);

  // Don't notify when there aren't any changes.
  EXPECT_CALL(observer_, OnSettingsChangedJSON(_, _, _, _)).Times(0);
  SetCurrentPolicy(policies);
  Mock::VerifyAndClearExpectations(&observer_);
}

}  // namespace extensions
