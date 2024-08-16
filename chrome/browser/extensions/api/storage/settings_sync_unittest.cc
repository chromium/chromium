// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/storage/settings_sync_util.h"
#include "chrome/browser/extensions/api/storage/sync_storage_backend.h"
#include "chrome/browser/extensions/api/storage/sync_value_store_cache.h"
#include "chrome/browser/extensions/api/storage/syncable_settings_storage.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync/model/sync_change_processor.h"
#include "components/sync/test/sync_change_processor_wrapper_for_test.h"
#include "components/value_store/test_value_store_factory.h"
#include "components/value_store/testing_value_store.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/storage/backend_task_runner.h"
#include "extensions/browser/api/storage/settings_test_util.h"
#include "extensions/browser/api/storage/storage_frontend.h"
#include "extensions/browser/api/storage/value_store_util.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/mock_extension_system.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest.h"
#include "testing/gtest/include/gtest/gtest.h"

using value_store::ValueStore;

namespace extensions {

namespace {

// To save typing ValueStore::DEFAULTS everywhere.
const ValueStore::WriteOptions DEFAULTS = ValueStore::DEFAULTS;

// More saving typing. Maps extension IDs to a list of sync changes for that
// extension.
using SettingSyncDataMultimap =
    std::map<ExtensionId, std::unique_ptr<SettingSyncDataList>>;

// Gets the pretty-printed JSON for a value.
static std::string GetJson(const base::Value& value) {
  std::string json;
  base::JSONWriter::WriteWithOptions(
      value, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);
  return json;
}

// Returns whether two Values are equal.
testing::AssertionResult ValuesEq(
    const char* _1, const char* _2,
    const base::Value* expected,
    const base::Value* actual) {
  if (expected == actual) {
    return testing::AssertionSuccess();
  }
  if (!expected && actual) {
    return testing::AssertionFailure() <<
        "Expected NULL, actual: " << GetJson(*actual);
  }
  if (expected && !actual) {
    return testing::AssertionFailure() <<
        "Expected: " << GetJson(*expected) << ", actual NULL";
  }
  if (*expected != *actual) {
    return testing::AssertionFailure() <<
        "Expected: " << GetJson(*expected) << ", actual: " << GetJson(*actual);
  }
  return testing::AssertionSuccess();
}

// Returns whether the result of a storage operation is an expected value.
// Logs when different.
testing::AssertionResult SettingsEq(const char* _1,
                                    const char* _2,
                                    const base::Value::Dict& expected,
                                    ValueStore::ReadResult actual) {
  if (!actual.status().ok()) {
    return testing::AssertionFailure()
           << "Expected: " << expected
           << ", actual has error: " << actual.status().message;
  }
  base::Value settings(actual.PassSettings());
  base::Value expected_value(expected.Clone());
  return ValuesEq(_1, _2, &expected_value, &settings);
}

// SyncChangeProcessor which just records the changes made, accessed after
// being converted to the more useful SettingSyncData via changes().
class MockSyncChangeProcessor : public syncer::SyncChangeProcessor {
 public:
  MockSyncChangeProcessor() : fail_all_requests_(false) {}

  // syncer::SyncChangeProcessor implementation.
  std::optional<syncer::ModelError> ProcessSyncChanges(
      const base::Location& from_here,
      const syncer::SyncChangeList& change_list) override {
    if (fail_all_requests_) {
      return syncer::ModelError(FROM_HERE,
                                "MockSyncChangeProcessor: configured to fail");
    }
    for (const auto& sync_change : change_list) {
      changes_.push_back(std::make_unique<SettingSyncData>(sync_change));
    }
    return std::nullopt;
  }

  // Mock methods.

  const SettingSyncDataList& changes() { return changes_; }

  void ClearChanges() {
    changes_.clear();
  }

  void set_fail_all_requests(bool fail_all_requests) {
    fail_all_requests_ = fail_all_requests;
  }

  // Returns the only change for a given extension setting.  If there is not
  // exactly 1 change for that key, a test assertion will fail.
  SettingSyncData* GetOnlyChange(const ExtensionId& extension_id,
                                 const std::string& key) {
    std::vector<SettingSyncData*> matching_changes;
    for (const std::unique_ptr<SettingSyncData>& change : changes_) {
      if (change->extension_id() == extension_id && change->key() == key)
        matching_changes.push_back(change.get());
    }
    if (matching_changes.empty()) {
      ADD_FAILURE() << "No matching changes for " << extension_id << "/" <<
          key << " (out of " << changes_.size() << ")";
      return nullptr;
    }
    if (matching_changes.size() != 1u) {
      ADD_FAILURE() << matching_changes.size() << " matching changes for " <<
           extension_id << "/" << key << " (out of " << changes_.size() << ")";
    }
    return matching_changes[0];
  }

 private:
  SettingSyncDataList changes_;
  bool fail_all_requests_;
};

std::unique_ptr<KeyedService> MockExtensionSystemFactoryFunction(
    content::BrowserContext* context) {
  return std::make_unique<MockExtensionSystem>(context);
}

std::unique_ptr<KeyedService> BuildEventRouter(
    content::BrowserContext* profile) {
  return std::make_unique<extensions::EventRouter>(profile, nullptr);
}

}  // namespace

class ExtensionSettingsSyncTest : public testing::Test {
 public:
  ExtensionSettingsSyncTest()
      : storage_factory_(new value_store::TestValueStoreFactory()),
        sync_processor_(new MockSyncChangeProcessor),
        sync_processor_wrapper_(new syncer::SyncChangeProcessorWrapperForTest(
            sync_processor_.get())) {}

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    profile_ = std::make_unique<TestingProfile>(temp_dir_.GetPath());
    content::RunAllTasksUntilIdle();

    storage_factory_->Reset();
    frontend_ =
        StorageFrontend::CreateForTesting(storage_factory_, profile_.get());

    ExtensionsBrowserClient::Get()
        ->GetExtensionSystemFactory()
        ->SetTestingFactoryAndUse(
            profile_.get(),
            base::BindRepeating(&MockExtensionSystemFactoryFunction));

    EventRouterFactory::GetInstance()->SetTestingFactory(
        profile_.get(), base::BindRepeating(&BuildEventRouter));

    // Hold a pointer to SyncValueStoreCache in the main thread, such that
    // GetSyncableService() can be called from the backend sequence.
    sync_cache_ = static_cast<SyncValueStoreCache*>(
        frontend_->GetValueStoreCache(settings_namespace::SYNC));
    ASSERT_NE(sync_cache_, nullptr);
  }

  void TearDown() override {
    frontend_.reset();
    profile_.reset();
    // Execute any pending deletion tasks.
    content::RunAllTasksUntilIdle();
  }

 protected:
  // Adds a record of an extension or app to the extension service, then returns
  // its storage area.
  ValueStore* AddExtensionAndGetStorage(const ExtensionId& id,
                                        Manifest::Type type) {
    scoped_refptr<const Extension> extension =
        settings_test_util::AddExtensionWithId(profile_.get(), id, type);
    return settings_test_util::GetStorage(extension, frontend_.get());
  }

  // Gets the syncer::SyncableService for the given sync type.
  SyncStorageBackend* GetSyncableService(syncer::DataType data_type) {
    // SyncValueStoreCache::GetSyncableService internally enforces |data_type|
    // to be APP_SETTINGS or EXTENSION_SETTINGS, and the dynamic type of the
    // returned service is always SyncStorageBackend, so it can be downcast.
    DCHECK(data_type == syncer::APP_SETTINGS ||
           data_type == syncer::EXTENSION_SETTINGS);
    return static_cast<SyncStorageBackend*>(
        sync_cache_->GetSyncableService(data_type));
  }

  // Gets all the sync data from the SyncableService for a sync type as a map
  // from extension id to its sync data.
  SettingSyncDataMultimap GetAllSyncData(syncer::DataType data_type) {
    syncer::SyncDataList as_list =
        GetSyncableService(data_type)->GetAllSyncDataForTesting(data_type);
    SettingSyncDataMultimap as_map;
    for (auto& data : as_list) {
      std::unique_ptr<SettingSyncData> sync_data(new SettingSyncData(data));
      std::unique_ptr<SettingSyncDataList>& list_for_extension =
          as_map[sync_data->extension_id()];
      if (!list_for_extension)
        list_for_extension = std::make_unique<SettingSyncDataList>();
      list_for_extension->push_back(std::move(sync_data));
    }
    return as_map;
  }

  // This class uses it's TestingValueStore in such a way that it always mints
  // new TestingValueStore instances.
  value_store::TestingValueStore* GetExisting(const ExtensionId& extension_id,
                                              syncer::DataType type) {
    base::FilePath value_store_dir;
    value_store_util::ModelType data_type;
    switch (type) {
      case syncer::APP_SETTINGS:
        data_type = value_store_util::ModelType::APP;
        break;
      case syncer::EXTENSION_SETTINGS:
        data_type = value_store_util::ModelType::EXTENSION;
        break;
      default:
        NOTREACHED_IN_MIGRATION();
        return nullptr;
    }
    value_store_dir = value_store_util::GetValueStoreDir(
        settings_namespace::SYNC, data_type, extension_id);
    return static_cast<value_store::TestingValueStore*>(
        storage_factory_->GetExisting(value_store_dir));
  }

  template <typename Func>
  static void RunFunc(Func func) {
    func();
  }

  template <typename Func>
  void PostOnBackendSequenceAndWait(const base::Location& from_here,
                                    Func func) {
    GetBackendTaskRunner()->PostTask(
        from_here,
        base::BindOnce(&ExtensionSettingsSyncTest::RunFunc<Func>, func));
    content::RunAllTasksUntilIdle();
  }

  // Needed so that the DCHECKs for running on FILE or UI threads pass.
  content::BrowserTaskEnvironment task_environment_;

  base::ScopedTempDir temp_dir_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<StorageFrontend> frontend_;
  scoped_refptr<value_store::TestValueStoreFactory> storage_factory_;
  std::unique_ptr<MockSyncChangeProcessor> sync_processor_;
  std::unique_ptr<syncer::SyncChangeProcessorWrapperForTest>
      sync_processor_wrapper_;
  raw_ptr<SyncValueStoreCache, DanglingUntriaged> sync_cache_;
};

// Get a semblance of coverage for both EXTENSION_SETTINGS and APP_SETTINGS
// sync by roughly alternative which one to test.

TEST_F(ExtensionSettingsSyncTest, NoDataDoesNotInvokeSync) {
  syncer::DataType data_type = syncer::EXTENSION_SETTINGS;
  Manifest::Type type = Manifest::TYPE_EXTENSION;

  PostOnBackendSequenceAndWait(FROM_HERE, [&, this]() {
    EXPECT_EQ(0u, GetAllSyncData(data_type).size());
  });

  // Have one extension created before sync is set up, the other created after.
  AddExtensionAndGetStorage("s1", type);

  PostOnBackendSequenceAndWait(FROM_HERE, [&, this]() {
    EXPECT_EQ(0u, GetAllSyncData(data_type).size());

    GetSyncableService(data_type)->MergeDataAndStartSyncing(
        data_type, syncer::SyncDataList(), std::move(sync_processor_wrapper_));
  });

  AddExtensionAndGetStorage("s2", type);

  PostOnBackendSequenceAndWait(FROM_HERE, [&, this]() {
    EXPECT_EQ(0u, GetAllSyncData(data_type).size());

    GetSyncableService(data_type)->StopSyncing(data_type);

    EXPECT_EQ(0u, sync_processor_->changes().size());
    EXPECT_EQ(0u, GetAllSyncData(data_type).size());
  });
}

TEST_F(ExtensionSettingsSyncTest, InSyncDataDoesNotInvokeSync) {
  syncer::DataType data_type = syncer::APP_SETTINGS;
  Manifest::Type type = Manifest::TYPE_LEGACY_PACKAGED_APP;

  base::Value value1("fooValue");
  base::Value value2(base::Value::Type::LIST);
  value2.GetList().Append("barValue");

  ValueStore* storage1 = AddExtensionAndGetStorage("s1", type);
  ValueStore* storage2 = AddExtensionAndGetStorage("s2", type);

  PostOnBackendSequenceAndWait(FROM_HERE, [&, this]() {
    storage1->Set(DEFAULTS, "foo", value1);
    storage2->Set(DEFAULTS, "bar", value2);

    SettingSyncDataMultimap all_sync_data = GetAllSyncData(data_type);
    EXPECT_EQ(2u, all_sync_data.size());
    EXPECT_EQ(1u, all_sync_data["s1"]->size());
    EXPECT_PRED_FORMAT2(ValuesEq, &value1, &(*all_sync_data["s1"])[0]->value());
    EXPECT_EQ(1u, all_sync_data["s2"]->size());
    EXPECT_PRED_FORMAT2(ValuesEq, &value2, &(*all_sync_data["s2"])[0]->value());

    syncer::SyncDataList sync_data;
    sync_data.push_back(
        settings_sync_util::CreateData("s1", "foo", value1, data_type));
    sync_data.push_back(
        settings_sync_util::CreateData("s2", "bar", value2, data_type));

    GetSyncableService(data_type)->MergeDataAndStartSyncing(
        data_type, sync_data, std::move(sync_processor_wrapper_));

    // Already in sync, so no changes.
    EXPECT_EQ(0u, sync_processor_->changes().size());

    // Regression test: not-changing the synced value shouldn't result in a sync
    // change, and changing the synced value should result in an update.
    storage1->Set(DEFAULTS, "foo", value1);
    EXPECT_EQ(0u, sync_processor_->changes().size());

    storage1->Set(DEFAULTS, "foo", value2);
    EXPECT_EQ(1u, sync_processor_->changes().size());
    SettingSyncData* change = sync_processor_->GetOnlyChange("s1", "foo");
    EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE, change->change_type());
    EXPECT_EQ(value2, change->value());

    GetSyncableService(data_type)->StopSyncing(data_type);
  });
}

TEST_F(ExtensionSettingsSyncTest, LocalDataWithNoSyncDataIsPushedToSync) {
  syncer::DataType data_type = syncer::EXTENSION_SETTINGS;
  Manifest::Type type = Manifest::TYPE_EXTENSION;

  base::Value value1("fooValue");
  base::Value value2(base::Value::Type::LIST);
  value2.GetList().Append("barValue");

  ValueStore* storage1 = AddExtensionAndGetStorage("s1", type);
  ValueStore* storage2 = AddExtensionAndGetStorage("s2", type);

  PostOnBackendSequenceAndWait(FROM_HERE, [&, this]() {
    storage1->Set(DEFAULTS, "foo", value1);
    storage2->Set(DEFAULTS, "bar", value2);

    GetSyncableService(data_type)->MergeDataAndStartSyncing(
        data_type, syncer::SyncDataList(), std::move(sync_processor_wrapper_));

    // All settings should have been pushed to sync.
    EXPECT_EQ(2u, sync_processor_->changes().size());
    SettingSyncData* change = sync_processor_->GetOnlyChange("s1", "foo");
    EXPECT_EQ(syncer::SyncChange::ACTION_ADD, change->change_type());
    EXPECT_EQ(value1, change->value());
    change = sync_processor_->GetOnlyChange("s2", "bar");
    EXPECT_EQ(syncer::SyncChange::ACTION_ADD, change->change_type());
    EXPECT_EQ(value2, change->value());

    GetSyncableService(data_type)->StopSyncing(data_type);
  });
}

TEST_F(ExtensionSettingsSyncTest, AnySyncDataOverwritesLocalData) {
  syncer::DataType data_type = syncer::APP_SETTINGS;
  Manifest::Type type = Manifest::TYPE_LEGACY_PACKAGED_APP;

  base::Value value1("fooValue");
  base::Value value2(base::Value::Type::LIST);
  value2.GetList().Append("barValue");

  // Maintain dictionaries mirrored to the expected values of the settings in
  // each storage area.
  base::Value::Dict expected1, expected2;

  // Pre-populate one of the storage areas.
  ValueStore* storage1 = AddExtensionAndGetStorage("s1", type);

  PostOnBackendSequenceAndWait(FROM_HERE, [&, this]() {
    storage1->Set(DEFAULTS, "overwriteMe", value1);

    syncer::SyncDataList sync_data;
    sync_data.push_back(
        settings_sync_util::CreateData("s1", "foo", value1, data_type));
    sync_data.push_back(
        settings_sync_util::CreateData("s2", "bar", value2, data_type));
    GetSyncableService(data_type)->MergeDataAndStartSyncing(
        data_type, sync_data, std::move(sync_processor_wrapper_));
    expected1.Set("foo", value1.Clone());
    expected2.Set("bar", value2.Clone());
  });

  ValueStore* storage2 = AddExtensionAndGetStorage("s2", type);

  PostOnBackendSequenceAndWait(FROM_HERE, [&, this]() {
    // All changes should be local, so no sync changes.
    EXPECT_EQ(0u, sync_processor_->changes().size());

    // Sync settings should have been pushed to local settings.
    EXPECT_PRED_FORMAT2(SettingsEq, expected1, storage1->Get());
    EXPECT_PRED_FORMAT2(SettingsEq, expected2, storage2->Get());

    GetSyncableService(data_type)->StopSyncing(data_type);
  });
}

TEST_F(ExtensionSettingsSyncTest, ProcessSyncChanges) {
  syncer::DataType data_type = syncer::EXTENSION_SETTINGS;
  Manifest::Type type = Manifest::TYPE_EXTENSION;

  base::Value value1("fooValue");
  base::Value value2(base::Value::Type::LIST);
  value2.GetList().Append("barValue");

  // Make storage1 initialised from local data, storage2 initialised from sync.
  ValueStore* storage1 = AddExtensionAndGetStorage("s1", type);
  ValueStore* storage2 = AddExtensionAndGetStorage("s2", type);

  PostOnBackendSequenceAndWait(FROM_HERE, [&, this]() {
    // Maintain dictionaries mirrored to the expected values of the settings in
    // each storage area.
    base::Value::Dict expected1, expected2;

    storage1->Set(DEFAULTS, "foo", value1);
    expected1.Set("foo", value1.Clone());

    syncer::SyncDataList sync_data;
    sync_data.push_back(
        settings_sync_util::CreateData("s2", "bar", value2, data_type));

    GetSyncableService(data_type)->MergeDataAndStartSyncing(
        data_type, sync_data, std::move(sync_processor_wrapper_));
    expected2.Set("bar", value2.Clone());

    // Make sync add some settings.
    syncer::SyncChangeList change_list;
    change_list.push_back(
        settings_sync_util::CreateAdd("s1", "bar", value2, data_type));
    change_list.push_back(
        settings_sync_util::CreateAdd("s2", "foo", value1, data_type));
    GetSyncableService(data_type)->ProcessSyncChanges(FROM_HERE, change_list);
    expected1.Set("bar", value2.Clone());
    expected2.Set("foo", value1.Clone());

    EXPECT_PRED_FORMAT2(SettingsEq, expected1, storage1->Get());
    EXPECT_PRED_FORMAT2(SettingsEq, expected2, storage2->Get());

    // Make sync update some settings, storage1 the new setting, storage2 the
    // initial setting.
    change_list.clear();
    change_list.push_back(
        settings_sync_util::CreateUpdate("s1", "bar", value2, data_type));
    change_list.push_back(
        settings_sync_util::CreateUpdate("s2", "bar", value1, data_type));
    GetSyncableService(data_type)->ProcessSyncChanges(FROM_HERE, change_list);
    expected1.Set("bar", value2.Clone());
    expected2.Set("bar", value1.Clone());

    EXPECT_PRED_FORMAT2(SettingsEq, expected1, storage1->Get());
    EXPECT_PRED_FORMAT2(SettingsEq, expected2, storage2->Get());

    // Make sync remove some settings, storage1 the initial setting, storage2
    // the new setting.
    change_list.clear();
    change_list.push_back(
        settings_sync_util::CreateDelete("s1", "foo", data_type));
    change_list.push_back(
        settings_sync_util::CreateDelete("s2", "foo", data_type));
    GetSyncableService(data_type)->ProcessSyncChanges(FROM_HERE, change_list);
    expected1.Remove("foo");
    expected2.Remove("foo");

    EXPECT_PRED_FORMAT2(SettingsEq, expected1, storage1->Get());
    EXPECT_PRED_FORMAT2(SettingsEq, expected2, storage2->Get());

    GetSyncableService(data_type)->StopSyncing(data_type);
  });
}

TEST_F(ExtensionSettingsSyncTest, PushToSync) {
  syncer::DataType data_type = syncer::APP_SETTINGS;
  Manifest::Type type = Manifest::TYPE_LEGACY_PACKAGED_APP;

  base::Value value1("fooValue");
  base::Value value2(base::Value::Type::LIST);
  value2.GetList().Append("barValue");

  // Make storage1/2 initialised from local data, storage3/4 initialised from
  // sync.
  ValueStore* storage1 = AddExtensionAndGetStorage("s1", type);
  ValueStore* storage2 = AddExtensionAndGetStorage("s2", type);
  ValueStore* storage3 = AddExtensionAndGetStorage("s3", type);
  ValueStore* storage4 = AddExtensionAndGetStorage("s4", type);

  PostOnBackendSequenceAndWait(FROM_HERE, [&, this]() {
    storage1->Set(DEFAULTS, "foo", value1);
    storage2->Set(DEFAULTS, "foo", value1);

    syncer::SyncDataList sync_data;
    sync_data.push_back(
        settings_sync_util::CreateData("s3", "bar", value2, data_type));
    sync_data.push_back(
        settings_sync_util::CreateData("s4", "bar", value2, data_type));

    GetSyncableService(data_type)->MergeDataAndStartSyncing(
        data_type, sync_data, std::move(sync_processor_wrapper_));

    // Add something locally.
    storage1->Set(DEFAULTS, "bar", value2);
    storage2->Set(DEFAULTS, "bar", value2);
    storage3->Set(DEFAULTS, "foo", value1);
    storage4->Set(DEFAULTS, "foo", value1);

    SettingSyncData* change = sync_processor_->GetOnlyChange("s1", "bar");
    EXPECT_EQ(syncer::SyncChange::ACTION_ADD, change->change_type());
    EXPECT_EQ(value2, change->value());
    sync_processor_->GetOnlyChange("s2", "bar");
    EXPECT_EQ(syncer::SyncChange::ACTION_ADD, change->change_type());
    EXPECT_EQ(value2, change->value());
    change = sync_processor_->GetOnlyChange("s3", "foo");
    EXPECT_EQ(syncer::SyncChange::ACTION_ADD, change->change_type());
    EXPECT_EQ(value1, change->value());
    change = sync_processor_->GetOnlyChange("s4", "foo");
    EXPECT_EQ(syncer::SyncChange::ACTION_ADD, change->change_type());
    EXPECT_EQ(value1, change->value());

    // Change something locally, storage1/3 the new setting and storage2/4 the
    // initial setting, for all combinations of local vs sync intialisation and
    // new vs initial.
    sync_processor_->ClearChanges();
    storage1->Set(DEFAULTS, "bar", value1);
    storage2->Set(DEFAULTS, "foo", value2);
    storage3->Set(DEFAULTS, "bar", value1);
    storage4->Set(DEFAULTS, "foo", value2);

    change = sync_processor_->GetOnlyChange("s1", "bar");
    EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE, change->change_type());
    EXPECT_EQ(value1, change->value());
    change = sync_processor_->GetOnlyChange("s2", "foo");
    EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE, change->change_type());
    EXPECT_EQ(value2, change->value());
    change = sync_processor_->GetOnlyChange("s3", "bar");
    EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE, change->change_type());
    EXPECT_EQ(value1, change->value());
    change = sync_processor_->GetOnlyChange("s4", "foo");
    EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE, change->change_type());
    EXPECT_EQ(value2, change->value());

    // Remove something locally, storage1/3 the new setting and storage2/4 the
    // initial setting, for all combinations of local vs sync intialisation and
    // new vs initial.
    sync_processor_->ClearChanges();
    storage1->Remove("foo");
    storage2->Remove("bar");
    storage3->Remove("foo");
    storage4->Remove("bar");

    EXPECT_EQ(syncer::SyncChange::ACTION_DELETE,
              sync_processor_->GetOnlyChange("s1", "foo")->change_type());
    EXPECT_EQ(syncer::SyncChange::ACTION_DELETE,
              sync_processor_->GetOnlyChange("s2", "bar")->change_type());
    EXPECT_EQ(syncer::SyncChange::ACTION_DELETE,
              sync_processor_->GetOnlyChange("s3", "foo")->change_type());
    EXPECT_EQ(syncer::SyncChange::ACTION_DELETE,
              sync_processor_->GetOnlyChange("s4", "bar")->change_type());

    // Remove some nonexistent settings.
    sync_processor_->ClearChanges();
    storage1->Remove("foo");
    storage2->Remove("bar");
    storage3->Remove("foo");
    storage4->Remove("bar");

    EXPECT_EQ(0u, sync_processor_->changes().size());

    // Clear the rest of the settings.  Add the removed ones back first so that
    // more than one setting is cleared.
    storage1->Set(DEFAULTS, "foo", value1);
    storage2->Set(DEFAULTS, "bar", value2);
    storage3->Set(DEFAULTS, "foo", value1);
    storage4->Set(DEFAULTS, "bar", value2);

    sync_processor_->ClearChanges();
    storage1->Clear();
    storage2->Clear();
    storage3->Clear();
    storage4->Clear();

    EXPECT_EQ(syncer::SyncChange::ACTION_DELETE,
              sync_processor_->GetOnlyChange("s1", "foo")->change_type());
    EXPECT_EQ(syncer::SyncChange::ACTION_DELETE,
              sync_processor_->GetOnlyChange("s1", "bar")->change_type());
    EXPECT_EQ(syncer::SyncChange::ACTION_DELETE,
              sync_processor_->GetOnlyChange("s2", "foo")->change_type());
    EXPECT_EQ(syncer::SyncChange::ACTION_DELETE,
              sync_processor_->GetOnlyChange("s2", "bar")->change_type());
    EXPECT_EQ(syncer::SyncChange::ACTION_DELETE,
              sync_processor_->GetOnlyChange("s3", "foo")->change_type());
    EXPECT_EQ(syncer::SyncChange::ACTION_DELETE,
              sync_processor_->GetOnlyChange("s3", "bar")->change_type());
    EXPECT_EQ(syncer::SyncChange::ACTION_DELETE,
              sync_processor_->GetOnlyChange("s4", "foo")->change_type());
    EXPECT_EQ(syncer::SyncChange::ACTION_DELETE,
              sync_processor_->GetOnlyChange("s4", "bar")->change_type());

    GetSyncableService(data_type)->StopSyncing(data_type);
  });
}

TEST_F(ExtensionSettingsSyncTest, ExtensionAndAppSettingsSyncSeparately) {
  base::Value value1("fooValue");
  base::Value value2(base::Value::Type::LIST);
  value2.GetList().Append("barValue");

  // storage1 is an extension, storage2 is an app.
  ValueStore* storage1 = AddExtensionAndGetStorage(
      "s1", Manifest::TYPE_EXTENSION);
  ValueStore* storage2 = AddExtensionAndGetStorage(
      "s2", Manifest::TYPE_LEGACY_PACKAGED_APP);

  PostOnBackendSequenceAndWait(FROM_HERE, [&, this]() {
    storage1->Set(DEFAULTS, "foo", value1);
    storage2->Set(DEFAULTS, "bar", value2);

    SettingSyncDataMultimap extension_sync_data =
        GetAllSyncData(syncer::EXTENSION_SETTINGS);
    EXPECT_EQ(1u, extension_sync_data.size());
    EXPECT_EQ(1u, extension_sync_data["s1"]->size());
    EXPECT_PRED_FORMAT2(ValuesEq, &value1,
                        &(*extension_sync_data["s1"])[0]->value());

    SettingSyncDataMultimap app_sync_data =
        GetAllSyncData(syncer::APP_SETTINGS);
    EXPECT_EQ(1u, app_sync_data.size());
    EXPECT_EQ(1u, app_sync_data["s2"]->size());
    EXPECT_PRED_FORMAT2(ValuesEq, &value2, &(*app_sync_data["s2"])[0]->value());

    // Stop each separately, there should be no changes either time.
    syncer::SyncDataList sync_data;
    sync_data.push_back(settings_sync_util::CreateData(
        "s1", "foo", value1, syncer::EXTENSION_SETTINGS));

    GetSyncableService(syncer::EXTENSION_SETTINGS)
        ->MergeDataAndStartSyncing(syncer::EXTENSION_SETTINGS, sync_data,
                                   std::move(sync_processor_wrapper_));
    GetSyncableService(syncer::EXTENSION_SETTINGS)
        ->StopSyncing(syncer::EXTENSION_SETTINGS);
    EXPECT_EQ(0u, sync_processor_->changes().size());

    sync_data.clear();
    sync_data.push_back(settings_sync_util::CreateData("s2", "bar", value2,
                                                       syncer::APP_SETTINGS));

    std::unique_ptr<syncer::SyncChangeProcessorWrapperForTest>
        app_settings_delegate_(new syncer::SyncChangeProcessorWrapperForTest(
            sync_processor_.get()));
    GetSyncableService(syncer::APP_SETTINGS)
        ->MergeDataAndStartSyncing(syncer::APP_SETTINGS, sync_data,
                                   std::move(app_settings_delegate_));
    GetSyncableService(syncer::APP_SETTINGS)->StopSyncing(syncer::APP_SETTINGS);
    EXPECT_EQ(0u, sync_processor_->changes().size());
  });
}

TEST_F(ExtensionSettingsSyncTest, FailingStartSyncingDisablesSync) {
  syncer::DataType data_type = syncer::EXTENSION_SETTINGS;
  Manifest::Type type = Manifest::TYPE_EXTENSION;

  base::Value fooValue("fooValue");
  base::Value barValue("barValue");

  // There is a bit of a convoluted method to get storage areas that can fail;
  // hand out TestingValueStore object then toggle them failing/succeeding
  // as necessary.

  ValueStore* good = AddExtensionAndGetStorage("good", type);
  ValueStore* bad = AddExtensionAndGetStorage("bad", type);

  PostOnBackendSequenceAndWait(FROM_HERE, [&, this]() {
    // Make bad fail for incoming sync changes.
    GetExisting("bad", data_type)->set_status_code(ValueStore::CORRUPTION);
    {
      syncer::SyncDataList sync_data;
      sync_data.push_back(
          settings_sync_util::CreateData("good", "foo", fooValue, data_type));
      sync_data.push_back(
          settings_sync_util::CreateData("bad", "foo", fooValue, data_type));
      GetSyncableService(data_type)->MergeDataAndStartSyncing(
          data_type, sync_data, std::move(sync_processor_wrapper_));
    }
    GetExisting("bad", data_type)->set_status_code(ValueStore::OK);

    {
      base::Value::Dict dict;
      dict.Set("foo", fooValue.Clone());
      EXPECT_PRED_FORMAT2(SettingsEq, dict, good->Get());
    }
    {
      base::Value::Dict dict;
      EXPECT_PRED_FORMAT2(SettingsEq, dict, bad->Get());
    }

    // Changes made to good should be sent to sync, changes from bad shouldn't.
    sync_processor_->ClearChanges();
    good->Set(DEFAULTS, "bar", barValue);
    bad->Set(DEFAULTS, "bar", barValue);

    EXPECT_EQ(syncer::SyncChange::ACTION_ADD,
              sync_processor_->GetOnlyChange("good", "bar")->change_type());
    EXPECT_EQ(1u, sync_processor_->changes().size());

    {
      base::Value::Dict dict;
      dict.Set("foo", fooValue.Clone());
      dict.Set("bar", barValue.Clone());
      EXPECT_PRED_FORMAT2(SettingsEq, dict, good->Get());
    }
    {
      base::Value::Dict dict;
      dict.Set("bar", barValue.Clone());
      EXPECT_PRED_FORMAT2(SettingsEq, dict, bad->Get());
    }

    // Changes received from sync should go to good but not bad (even when it's
    // not failing).
    {
      syncer::SyncChangeList change_list;
      change_list.push_back(
          settings_sync_util::CreateUpdate("good", "foo", barValue, data_type));
      // (Sending UPDATE here even though it's adding, since that's what the
      // state of sync is.  In any case, it won't work.)
      change_list.push_back(
          settings_sync_util::CreateUpdate("bad", "foo", barValue, data_type));
      GetSyncableService(data_type)->ProcessSyncChanges(FROM_HERE, change_list);
    }

    {
      base::Value::Dict dict;
      dict.Set("foo", barValue.Clone());
      dict.Set("bar", barValue.Clone());
      EXPECT_PRED_FORMAT2(SettingsEq, dict, good->Get());
    }
    {
      base::Value::Dict dict;
      dict.Set("bar", barValue.Clone());
      EXPECT_PRED_FORMAT2(SettingsEq, dict, bad->Get());
    }

    // Changes made to bad still shouldn't go to sync, even though it didn't
    // fail last time.
    sync_processor_->ClearChanges();
    good->Set(DEFAULTS, "bar", fooValue);
    bad->Set(DEFAULTS, "bar", fooValue);

    EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE,
              sync_processor_->GetOnlyChange("good", "bar")->change_type());
    EXPECT_EQ(1u, sync_processor_->changes().size());

    {
      base::Value::Dict dict;
      dict.Set("foo", barValue.Clone());
      dict.Set("bar", fooValue.Clone());
      EXPECT_PRED_FORMAT2(SettingsEq, dict, good->Get());
    }
    {
      base::Value::Dict dict;
      dict.Set("bar", fooValue.Clone());
      EXPECT_PRED_FORMAT2(SettingsEq, dict, bad->Get());
    }

    // Failing ProcessSyncChanges shouldn't go to the storage.
    GetExisting("bad", data_type)->set_status_code(ValueStore::CORRUPTION);
    {
      syncer::SyncChangeList change_list;
      change_list.push_back(
          settings_sync_util::CreateUpdate("good", "foo", fooValue, data_type));
      // (Ditto.)
      change_list.push_back(
          settings_sync_util::CreateUpdate("bad", "foo", fooValue, data_type));
      GetSyncableService(data_type)->ProcessSyncChanges(FROM_HERE, change_list);
    }
    GetExisting("bad", data_type)->set_status_code(ValueStore::OK);

    {
      base::Value::Dict dict;
      dict.Set("foo", fooValue.Clone());
      dict.Set("bar", fooValue.Clone());
      EXPECT_PRED_FORMAT2(SettingsEq, dict, good->Get());
    }
    {
      base::Value::Dict dict;
      dict.Set("bar", fooValue.Clone());
      EXPECT_PRED_FORMAT2(SettingsEq, dict, bad->Get());
    }

    // Restarting sync should make bad start syncing again.
    sync_processor_->ClearChanges();
    GetSyncableService(data_type)->StopSyncing(data_type);
    sync_processor_wrapper_ =
        std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
            sync_processor_.get());
    GetSyncableService(data_type)->MergeDataAndStartSyncing(
        data_type, syncer::SyncDataList(), std::move(sync_processor_wrapper_));

    // Local settings will have been pushed to sync, since it's empty (in this
    // test; presumably it wouldn't be live, since we've been getting changes).
    EXPECT_EQ(syncer::SyncChange::ACTION_ADD,
              sync_processor_->GetOnlyChange("good", "foo")->change_type());
    EXPECT_EQ(syncer::SyncChange::ACTION_ADD,
              sync_processor_->GetOnlyChange("good", "bar")->change_type());
    EXPECT_EQ(syncer::SyncChange::ACTION_ADD,
              sync_processor_->GetOnlyChange("bad", "bar")->change_type());
    EXPECT_EQ(3u, sync_processor_->changes().size());

    // Live local changes now get pushed, too.
    sync_processor_->ClearChanges();
    good->Set(DEFAULTS, "bar", barValue);
    bad->Set(DEFAULTS, "bar", barValue);

    EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE,
              sync_processor_->GetOnlyChange("good", "bar")->change_type());
    EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE,
              sync_processor_->GetOnlyChange("bad", "bar")->change_type());
    EXPECT_EQ(2u, sync_processor_->changes().size());

    // And ProcessSyncChanges work, too.
    {
      syncer::SyncChangeList change_list;
      change_list.push_back(
          settings_sync_util::CreateUpdate("good", "bar", fooValue, data_type));
      change_list.push_back(
          settings_sync_util::CreateUpdate("bad", "bar", fooValue, data_type));
      GetSyncableService(data_type)->ProcessSyncChanges(FROM_HERE, change_list);
    }

    {
      base::Value::Dict dict;
      dict.Set("foo", fooValue.Clone());
      dict.Set("bar", fooValue.Clone());
      EXPECT_PRED_FORMAT2(SettingsEq, dict, good->Get());
    }
    {
      base::Value::Dict dict;
      dict.Set("bar", fooValue.Clone());
      EXPECT_PRED_FORMAT2(SettingsEq, dict, bad->Get());
    }
  });
}

TEST_F(ExtensionSettingsSyncTest, FailingProcessChangesDisablesSync) {
  // The test above tests a failing ProcessSyncChanges too, but here test with
  // an initially passing MergeDataAndStartSyncing.
  syncer::DataType data_type = syncer::APP_SETTINGS;
  Manifest::Type type = Manifest::TYPE_LEGACY_PACKAGED_APP;

  base::Value fooValue("fooValue");
  base::Value barValue("barValue");

  ValueStore* good = AddExtensionAndGetStorage("good", type);
  ValueStore* bad = AddExtensionAndGetStorage("bad", type);

  PostOnBackendSequenceAndWait(FROM_HERE, [&, this]() {
    // Unlike before, initially succeeding MergeDataAndStartSyncing.
    {
      syncer::SyncDataList sync_data;
      sync_data.push_back(
          settings_sync_util::CreateData("good", "foo", fooValue, data_type));
      sync_data.push_back(
          settings_sync_util::CreateData("bad", "foo", fooValue, data_type));
      GetSyncableService(data_type)->MergeDataAndStartSyncing(
          data_type, sync_data, std::move(sync_processor_wrapper_));
    }

    EXPECT_EQ(0u, sync_processor_->changes().size());

    {
      base::Value::Dict dict;
      dict.Set("foo", fooValue.Clone());
      EXPECT_PRED_FORMAT2(SettingsEq, dict, good->Get());
    }
    {
      base::Value::Dict dict;
      dict.Set("foo", fooValue.Clone());
      EXPECT_PRED_FORMAT2(SettingsEq, dict, bad->Get());
    }

    // Now fail ProcessSyncChanges for bad.
    GetExisting("bad", data_type)->set_status_code(ValueStore::CORRUPTION);
    {
      syncer::SyncChangeList change_list;
      change_list.push_back(
          settings_sync_util::CreateAdd("good", "bar", barValue, data_type));
      change_list.push_back(
          settings_sync_util::CreateAdd("bad", "bar", barValue, data_type));
      GetSyncableService(data_type)->ProcessSyncChanges(FROM_HERE, change_list);
    }
    GetExisting("bad", data_type)->set_status_code(ValueStore::OK);

    {
      base::Value::Dict dict;
      dict.Set("foo", fooValue.Clone());
      dict.Set("bar", barValue.Clone());
      EXPECT_PRED_FORMAT2(SettingsEq, dict, good->Get());
    }
    {
      base::Value::Dict dict;
      dict.Set("foo", fooValue.Clone());
      EXPECT_PRED_FORMAT2(SettingsEq, dict, bad->Get());
    }

    // No more changes sent to sync for bad.
    sync_processor_->ClearChanges();
    good->Set(DEFAULTS, "foo", barValue);
    bad->Set(DEFAULTS, "foo", barValue);

    EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE,
              sync_processor_->GetOnlyChange("good", "foo")->change_type());
    EXPECT_EQ(1u, sync_processor_->changes().size());

    // No more changes received from sync should go to bad.
    {
      syncer::SyncChangeList change_list;
      change_list.push_back(
          settings_sync_util::CreateAdd("good", "foo", fooValue, data_type));
      change_list.push_back(
          settings_sync_util::CreateAdd("bad", "foo", fooValue, data_type));
      GetSyncableService(data_type)->ProcessSyncChanges(FROM_HERE, change_list);
    }

    {
      base::Value::Dict dict;
      dict.Set("foo", fooValue.Clone());
      dict.Set("bar", barValue.Clone());
      EXPECT_PRED_FORMAT2(SettingsEq, dict, good->Get());
    }
    {
      base::Value::Dict dict;
      dict.Set("foo", barValue.Clone());
      EXPECT_PRED_FORMAT2(SettingsEq, dict, bad->Get());
    }
  });
}

TEST_F(ExtensionSettingsSyncTest, FailingGetAllSyncDataDoesntStopSync) {
  syncer::DataType data_type = syncer::EXTENSION_SETTINGS;
  Manifest::Type type = Manifest::TYPE_EXTENSION;

  base::Value fooValue("fooValue");
  base::Value barValue("barValue");

  ValueStore* good = AddExtensionAndGetStorage("good", type);
  ValueStore* bad = AddExtensionAndGetStorage("bad", type);

  PostOnBackendSequenceAndWait(FROM_HERE, [&, this]() {
    good->Set(DEFAULTS, "foo", fooValue);
    bad->Set(DEFAULTS, "foo", fooValue);

    // Even though bad will fail to get all sync data, sync data should still
    // include that from good.
    GetExisting("bad", data_type)->set_status_code(ValueStore::CORRUPTION);
    {
      syncer::SyncDataList all_sync_data =
          GetSyncableService(data_type)->GetAllSyncDataForTesting(data_type);
      EXPECT_EQ(1u, all_sync_data.size());
      EXPECT_EQ(syncer::ClientTagHash::FromUnhashed(data_type, "good/foo"),
                all_sync_data[0].GetClientTagHash());
    }
    GetExisting("bad", data_type)->set_status_code(ValueStore::OK);

    // Sync shouldn't be disabled for good (nor bad -- but this is unimportant).
    GetSyncableService(data_type)->MergeDataAndStartSyncing(
        data_type, syncer::SyncDataList(), std::move(sync_processor_wrapper_));

    EXPECT_EQ(syncer::SyncChange::ACTION_ADD,
              sync_processor_->GetOnlyChange("good", "foo")->change_type());
    EXPECT_EQ(syncer::SyncChange::ACTION_ADD,
              sync_processor_->GetOnlyChange("bad", "foo")->change_type());
    EXPECT_EQ(2u, sync_processor_->changes().size());

    sync_processor_->ClearChanges();
    good->Set(DEFAULTS, "bar", barValue);
    bad->Set(DEFAULTS, "bar", barValue);

    EXPECT_EQ(syncer::SyncChange::ACTION_ADD,
              sync_processor_->GetOnlyChange("good", "bar")->change_type());
    EXPECT_EQ(syncer::SyncChange::ACTION_ADD,
              sync_processor_->GetOnlyChange("bad", "bar")->change_type());
    EXPECT_EQ(2u, sync_processor_->changes().size());
  });
}

TEST_F(ExtensionSettingsSyncTest, FailureToReadChangesToPushDisablesSync) {
  syncer::DataType data_type = syncer::APP_SETTINGS;
  Manifest::Type type = Manifest::TYPE_LEGACY_PACKAGED_APP;

  base::Value fooValue("fooValue");
  base::Value barValue("barValue");

  ValueStore* good = AddExtensionAndGetStorage("good", type);
  ValueStore* bad = AddExtensionAndGetStorage("bad", type);

  PostOnBackendSequenceAndWait(FROM_HERE, [&, this]() {
    good->Set(DEFAULTS, "foo", fooValue);
    bad->Set(DEFAULTS, "foo", fooValue);

    // good will successfully push foo:fooValue to sync, but bad will fail to
    // get them so won't.
    GetExisting("bad", data_type)->set_status_code(ValueStore::CORRUPTION);
    GetSyncableService(data_type)->MergeDataAndStartSyncing(
        data_type, syncer::SyncDataList(), std::move(sync_processor_wrapper_));
    GetExisting("bad", data_type)->set_status_code(ValueStore::OK);

    EXPECT_EQ(syncer::SyncChange::ACTION_ADD,
              sync_processor_->GetOnlyChange("good", "foo")->change_type());
    EXPECT_EQ(1u, sync_processor_->changes().size());

    // bad should now be disabled for sync.
    sync_processor_->ClearChanges();
    good->Set(DEFAULTS, "bar", barValue);
    bad->Set(DEFAULTS, "bar", barValue);

    EXPECT_EQ(syncer::SyncChange::ACTION_ADD,
              sync_processor_->GetOnlyChange("good", "bar")->change_type());
    EXPECT_EQ(1u, sync_processor_->changes().size());

    {
      syncer::SyncChangeList change_list;
      change_list.push_back(
          settings_sync_util::CreateUpdate("good", "foo", barValue, data_type));
      // (Sending ADD here even though it's updating, since that's what the
      // state of sync is.  In any case, it won't work.)
      change_list.push_back(
          settings_sync_util::CreateAdd("bad", "foo", barValue, data_type));
      GetSyncableService(data_type)->ProcessSyncChanges(FROM_HERE, change_list);
    }

    {
      base::Value::Dict dict;
      dict.Set("foo", barValue.Clone());
      dict.Set("bar", barValue.Clone());
      EXPECT_PRED_FORMAT2(SettingsEq, dict, good->Get());
    }
    {
      base::Value::Dict dict;
      dict.Set("foo", fooValue.Clone());
      dict.Set("bar", barValue.Clone());
      EXPECT_PRED_FORMAT2(SettingsEq, dict, bad->Get());
    }

    // Re-enabling sync without failing should cause the local changes from bad
    // to be pushed to sync successfully, as should future changes to bad.
    sync_processor_->ClearChanges();
    GetSyncableService(data_type)->StopSyncing(data_type);
    sync_processor_wrapper_ =
        std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
            sync_processor_.get());
    GetSyncableService(data_type)->MergeDataAndStartSyncing(
        data_type, syncer::SyncDataList(), std::move(sync_processor_wrapper_));

    EXPECT_EQ(syncer::SyncChange::ACTION_ADD,
              sync_processor_->GetOnlyChange("good", "foo")->change_type());
    EXPECT_EQ(syncer::SyncChange::ACTION_ADD,
              sync_processor_->GetOnlyChange("good", "bar")->change_type());
    EXPECT_EQ(syncer::SyncChange::ACTION_ADD,
              sync_processor_->GetOnlyChange("bad", "foo")->change_type());
    EXPECT_EQ(syncer::SyncChange::ACTION_ADD,
              sync_processor_->GetOnlyChange("bad", "bar")->change_type());
    EXPECT_EQ(4u, sync_processor_->changes().size());

    sync_processor_->ClearChanges();
    good->Set(DEFAULTS, "bar", fooValue);
    bad->Set(DEFAULTS, "bar", fooValue);

    EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE,
              sync_processor_->GetOnlyChange("good", "bar")->change_type());
    EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE,
              sync_processor_->GetOnlyChange("good", "bar")->change_type());
    EXPECT_EQ(2u, sync_processor_->changes().size());
  });
}

TEST_F(ExtensionSettingsSyncTest, FailureToPushLocalStateDisablesSync) {
  syncer::DataType data_type = syncer::EXTENSION_SETTINGS;
  Manifest::Type type = Manifest::TYPE_EXTENSION;

  base::Value fooValue("fooValue");
  base::Value barValue("barValue");

  ValueStore* good = AddExtensionAndGetStorage("good", type);
  ValueStore* bad = AddExtensionAndGetStorage("bad", type);

  PostOnBackendSequenceAndWait(FROM_HERE, [&, this]() {
    // Only set bad; setting good will cause it to fail below.
    bad->Set(DEFAULTS, "foo", fooValue);

    sync_processor_->set_fail_all_requests(true);
    GetSyncableService(data_type)->MergeDataAndStartSyncing(
        data_type, syncer::SyncDataList(), std::move(sync_processor_wrapper_));
    sync_processor_->set_fail_all_requests(false);

    // Changes from good will be send to sync, changes from bad won't.
    sync_processor_->ClearChanges();
    good->Set(DEFAULTS, "foo", barValue);
    bad->Set(DEFAULTS, "foo", barValue);

    EXPECT_EQ(syncer::SyncChange::ACTION_ADD,
              sync_processor_->GetOnlyChange("good", "foo")->change_type());
    EXPECT_EQ(1u, sync_processor_->changes().size());

    // Changes from sync will be sent to good, not to bad.
    {
      syncer::SyncChangeList change_list;
      change_list.push_back(
          settings_sync_util::CreateAdd("good", "bar", barValue, data_type));
      change_list.push_back(
          settings_sync_util::CreateAdd("bad", "bar", barValue, data_type));
      GetSyncableService(data_type)->ProcessSyncChanges(FROM_HERE, change_list);
    }

    {
      base::Value::Dict dict;
      dict.Set("foo", barValue.Clone());
      dict.Set("bar", barValue.Clone());
      EXPECT_PRED_FORMAT2(SettingsEq, dict, good->Get());
    }
    {
      base::Value::Dict dict;
      dict.Set("foo", barValue.Clone());
      EXPECT_PRED_FORMAT2(SettingsEq, dict, bad->Get());
    }

    // Restarting sync makes everything work again.
    sync_processor_->ClearChanges();
    GetSyncableService(data_type)->StopSyncing(data_type);
    sync_processor_wrapper_ =
        std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
            sync_processor_.get());
    GetSyncableService(data_type)->MergeDataAndStartSyncing(
        data_type, syncer::SyncDataList(), std::move(sync_processor_wrapper_));

    EXPECT_EQ(syncer::SyncChange::ACTION_ADD,
              sync_processor_->GetOnlyChange("good", "foo")->change_type());
    EXPECT_EQ(syncer::SyncChange::ACTION_ADD,
              sync_processor_->GetOnlyChange("good", "bar")->change_type());
    EXPECT_EQ(syncer::SyncChange::ACTION_ADD,
              sync_processor_->GetOnlyChange("bad", "foo")->change_type());
    EXPECT_EQ(3u, sync_processor_->changes().size());

    sync_processor_->ClearChanges();
    good->Set(DEFAULTS, "foo", fooValue);
    bad->Set(DEFAULTS, "foo", fooValue);

    EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE,
              sync_processor_->GetOnlyChange("good", "foo")->change_type());
    EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE,
              sync_processor_->GetOnlyChange("good", "foo")->change_type());
    EXPECT_EQ(2u, sync_processor_->changes().size());
  });
}

TEST_F(ExtensionSettingsSyncTest, FailureToPushLocalChangeDisablesSync) {
  syncer::DataType data_type = syncer::EXTENSION_SETTINGS;
  Manifest::Type type = Manifest::TYPE_EXTENSION;

  base::Value fooValue("fooValue");
  base::Value barValue("barValue");

  ValueStore* good = AddExtensionAndGetStorage("good", type);
  ValueStore* bad = AddExtensionAndGetStorage("bad", type);

  PostOnBackendSequenceAndWait(FROM_HERE, [&, this]() {
    GetSyncableService(data_type)->MergeDataAndStartSyncing(
        data_type, syncer::SyncDataList(), std::move(sync_processor_wrapper_));

    // bad will fail to send changes.
    good->Set(DEFAULTS, "foo", fooValue);
    sync_processor_->set_fail_all_requests(true);
    bad->Set(DEFAULTS, "foo", fooValue);
    sync_processor_->set_fail_all_requests(false);

    EXPECT_EQ(syncer::SyncChange::ACTION_ADD,
              sync_processor_->GetOnlyChange("good", "foo")->change_type());
    EXPECT_EQ(1u, sync_processor_->changes().size());

    // No further changes should be sent from bad.
    sync_processor_->ClearChanges();
    good->Set(DEFAULTS, "foo", barValue);
    bad->Set(DEFAULTS, "foo", barValue);

    EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE,
              sync_processor_->GetOnlyChange("good", "foo")->change_type());
    EXPECT_EQ(1u, sync_processor_->changes().size());

    // Changes from sync will be sent to good, not to bad.
    {
      syncer::SyncChangeList change_list;
      change_list.push_back(
          settings_sync_util::CreateAdd("good", "bar", barValue, data_type));
      change_list.push_back(
          settings_sync_util::CreateAdd("bad", "bar", barValue, data_type));
      GetSyncableService(data_type)->ProcessSyncChanges(FROM_HERE, change_list);
    }

    {
      base::Value::Dict dict;
      dict.Set("foo", barValue.Clone());
      dict.Set("bar", barValue.Clone());
      EXPECT_PRED_FORMAT2(SettingsEq, dict, good->Get());
    }
    {
      base::Value::Dict dict;
      dict.Set("foo", barValue.Clone());
      EXPECT_PRED_FORMAT2(SettingsEq, dict, bad->Get());
    }

    // Restarting sync makes everything work again.
    sync_processor_->ClearChanges();
    GetSyncableService(data_type)->StopSyncing(data_type);
    sync_processor_wrapper_ =
        std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
            sync_processor_.get());
    GetSyncableService(data_type)->MergeDataAndStartSyncing(
        data_type, syncer::SyncDataList(), std::move(sync_processor_wrapper_));

    EXPECT_EQ(syncer::SyncChange::ACTION_ADD,
              sync_processor_->GetOnlyChange("good", "foo")->change_type());
    EXPECT_EQ(syncer::SyncChange::ACTION_ADD,
              sync_processor_->GetOnlyChange("good", "bar")->change_type());
    EXPECT_EQ(syncer::SyncChange::ACTION_ADD,
              sync_processor_->GetOnlyChange("bad", "foo")->change_type());
    EXPECT_EQ(3u, sync_processor_->changes().size());

    sync_processor_->ClearChanges();
    good->Set(DEFAULTS, "foo", fooValue);
    bad->Set(DEFAULTS, "foo", fooValue);

    EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE,
              sync_processor_->GetOnlyChange("good", "foo")->change_type());
    EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE,
              sync_processor_->GetOnlyChange("good", "foo")->change_type());
    EXPECT_EQ(2u, sync_processor_->changes().size());
  });
}

TEST_F(ExtensionSettingsSyncTest,
       LargeOutgoingChangeRejectedButIncomingAccepted) {
  syncer::DataType data_type = syncer::APP_SETTINGS;
  Manifest::Type type = Manifest::TYPE_LEGACY_PACKAGED_APP;

  // This value should be larger than the limit in sync_storage_backend.cc.
  base::Value large_value(std::string(10000, 'a'));

  PostOnBackendSequenceAndWait(FROM_HERE, [&, this]() {
    GetSyncableService(data_type)->MergeDataAndStartSyncing(
        data_type, syncer::SyncDataList(), std::move(sync_processor_wrapper_));
  });

  // Large local change rejected and doesn't get sent out.
  ValueStore* storage1 = AddExtensionAndGetStorage("s1", type);

  PostOnBackendSequenceAndWait(FROM_HERE, [&, this]() {
    EXPECT_FALSE(
        storage1->Set(DEFAULTS, "large_value", large_value).status().ok());
    EXPECT_EQ(0u, sync_processor_->changes().size());
  });

  // Large incoming change should still get accepted.
  ValueStore* storage2 = AddExtensionAndGetStorage("s2", type);

  PostOnBackendSequenceAndWait(FROM_HERE, [&, this]() {
    {
      syncer::SyncChangeList change_list;
      change_list.push_back(settings_sync_util::CreateAdd(
          "s1", "large_value", large_value, data_type));
      change_list.push_back(settings_sync_util::CreateAdd(
          "s2", "large_value", large_value, data_type));
      GetSyncableService(data_type)->ProcessSyncChanges(FROM_HERE, change_list);
    }
    {
      base::Value::Dict expected;
      expected.Set("large_value", large_value.Clone());
      EXPECT_PRED_FORMAT2(SettingsEq, expected, storage1->Get());
      EXPECT_PRED_FORMAT2(SettingsEq, expected, storage2->Get());
    }

    GetSyncableService(data_type)->StopSyncing(data_type);
  });
}

TEST_F(ExtensionSettingsSyncTest, Dots) {
  syncer::DataType data_type = syncer::EXTENSION_SETTINGS;
  Manifest::Type type = Manifest::TYPE_EXTENSION;

  ValueStore* storage = AddExtensionAndGetStorage("ext", type);

  PostOnBackendSequenceAndWait(FROM_HERE, [&, this]() {
    {
      syncer::SyncDataList sync_data_list;
      std::unique_ptr<base::Value> string_value(new base::Value("value"));
      sync_data_list.push_back(settings_sync_util::CreateData(
          "ext", "key.with.dot", *string_value, data_type));

      GetSyncableService(data_type)->MergeDataAndStartSyncing(
          data_type, sync_data_list, std::move(sync_processor_wrapper_));
    }

    // Test dots in keys that come from sync.
    {
      ValueStore::ReadResult data = storage->Get();
      ASSERT_TRUE(data.status().ok());

      base::Value::Dict expected_data;
      expected_data.Set("key.with.dot", base::Value("value"));
      EXPECT_EQ(expected_data, data.settings());
    }

    // Test dots in keys going to sync.
    {
      std::unique_ptr<base::Value> string_value(new base::Value("spot"));
      storage->Set(DEFAULTS, "key.with.spot", *string_value);

      ASSERT_EQ(1u, sync_processor_->changes().size());
      SettingSyncData* sync_data = sync_processor_->changes()[0].get();
      EXPECT_EQ(syncer::SyncChange::ACTION_ADD, sync_data->change_type());
      EXPECT_EQ("ext", sync_data->extension_id());
      EXPECT_EQ("key.with.spot", sync_data->key());
      EXPECT_EQ(sync_data->value(), *string_value);
    }
  });
}

// In other (frontend) tests, we assume that the result of GetStorage
// is a pointer to the a Storage owned by a Frontend object, but for
// the unlimitedStorage case, this might not be true. So, write the
// tests in a "callback" style.  We should really rewrite all tests to
// be asynchronous in this way.

namespace {

static void UnlimitedSyncStorageTestCallback(ValueStore* sync_storage) {
  // Sync storage should still run out after ~100K; the unlimitedStorage
  // permission can't apply to sync.
  base::Value kilobyte = settings_test_util::CreateKilobyte();
  for (int i = 0; i < 100; ++i) {
    sync_storage->Set(ValueStore::DEFAULTS, base::NumberToString(i), kilobyte);
  }

  EXPECT_FALSE(sync_storage->Set(ValueStore::DEFAULTS, "WillError", kilobyte)
                   .status()
                   .ok());
}

static void UnlimitedLocalStorageTestCallback(ValueStore* local_storage) {
  // Local storage should never run out.
  base::Value megabyte = settings_test_util::CreateMegabyte();
  for (int i = 0; i < 7; ++i) {
    local_storage->Set(ValueStore::DEFAULTS, base::NumberToString(i), megabyte);
  }

  EXPECT_TRUE(local_storage->Set(ValueStore::DEFAULTS, "WontError", megabyte)
                  .status()
                  .ok());
}

}  // namespace

TEST_F(ExtensionSettingsSyncTest, UnlimitedStorageForLocalButNotSync) {
  const ExtensionId id = "ext";
  std::set<std::string> permissions;
  permissions.insert("unlimitedStorage");
  scoped_refptr<const Extension> extension =
      settings_test_util::AddExtensionWithIdAndPermissions(
          profile_.get(), id, Manifest::TYPE_EXTENSION, permissions);

  frontend_->RunWithStorage(extension, settings_namespace::SYNC,
                            base::BindOnce(&UnlimitedSyncStorageTestCallback));
  frontend_->RunWithStorage(extension, settings_namespace::LOCAL,
                            base::BindOnce(&UnlimitedLocalStorageTestCallback));

  content::RunAllTasksUntilIdle();
}

}  // namespace extensions
