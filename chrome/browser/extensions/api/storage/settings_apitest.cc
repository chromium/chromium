// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/storage/settings_sync_util.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/policy/schema_registry_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/core/common/schema_map.h"
#include "components/policy/core/common/schema_registry.h"
#include "components/sync/model/fake_sync_change_processor.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/model/sync_change_processor.h"
#include "components/sync/model/sync_change_processor_wrapper_for_test.h"
#include "components/sync/model/sync_error_factory.h"
#include "components/sync/model/sync_error_factory_mock.h"
#include "components/sync/model/syncable_service.h"
#include "extensions/browser/api/storage/backend_task_runner.h"
#include "extensions/browser/api/storage/settings_namespace.h"
#include "extensions/browser/api/storage/storage_frontend.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/value_builder.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace extensions {

using settings_namespace::LOCAL;
using settings_namespace::MANAGED;
using settings_namespace::Namespace;
using settings_namespace::SYNC;
using settings_namespace::ToString;
using testing::Mock;
using testing::Return;
using testing::_;

namespace {

// TODO(kalman): test both EXTENSION_SETTINGS and APP_SETTINGS.
const syncer::ModelType kModelType = syncer::EXTENSION_SETTINGS;

// The managed_storage extension has a key defined in its manifest, so that
// its extension ID is well-known and the policy system can push policies for
// the extension.
const char kManagedStorageExtensionId[] = "kjmkgkdkpedkejedfhmfcenooemhbpbo";

class MockSchemaRegistryObserver : public policy::SchemaRegistry::Observer {
 public:
  MockSchemaRegistryObserver() {}
  ~MockSchemaRegistryObserver() override {}

  MOCK_METHOD1(OnSchemaRegistryUpdated, void(bool));
};

}  // namespace

class ExtensionSettingsApiTest : public ExtensionApiTest {
 protected:
  void SetUpInProcessBrowserTestFixture() override {
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();

    EXPECT_CALL(policy_provider_, IsInitializationComplete(_))
        .WillRepeatedly(Return(true));
    policy_provider_.SetAutoRefresh();
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
  }

  void ReplyWhenSatisfied(
      Namespace settings_namespace,
      const std::string& normal_action,
      const std::string& incognito_action) {
    MaybeLoadAndReplyWhenSatisfied(
        settings_namespace, normal_action, incognito_action, NULL, false);
  }

  const Extension* LoadAndReplyWhenSatisfied(
      Namespace settings_namespace,
      const std::string& normal_action,
      const std::string& incognito_action,
      const std::string& extension_dir) {
    return MaybeLoadAndReplyWhenSatisfied(
        settings_namespace,
        normal_action,
        incognito_action,
        &extension_dir,
        false);
  }

  void FinalReplyWhenSatisfied(
      Namespace settings_namespace,
      const std::string& normal_action,
      const std::string& incognito_action) {
    MaybeLoadAndReplyWhenSatisfied(
        settings_namespace, normal_action, incognito_action, NULL, true);
  }

  static void InitSyncOnBackgroundSequence(
      base::OnceCallback<base::WeakPtr<syncer::SyncableService>()>
          syncable_service_provider,
      syncer::SyncChangeProcessor* sync_processor) {
    DCHECK(GetBackendTaskRunner()->RunsTasksInCurrentSequence());

    base::WeakPtr<syncer::SyncableService> syncable_service =
        std::move(syncable_service_provider).Run();
    DCHECK(syncable_service.get());
    EXPECT_FALSE(
        syncable_service
            ->MergeDataAndStartSyncing(
                kModelType, syncer::SyncDataList(),
                std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
                    sync_processor),
                std::make_unique<syncer::SyncErrorFactoryMock>())
            .error()
            .IsSet());
  }

  void InitSync(syncer::SyncChangeProcessor* sync_processor) {
    base::RunLoop().RunUntilIdle();

    base::RunLoop loop;
    GetBackendTaskRunner()->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&InitSyncOnBackgroundSequence,
                       settings_sync_util::GetSyncableServiceProvider(
                           profile(), kModelType),
                       sync_processor),
        loop.QuitClosure());
    loop.Run();
  }

  static void SendChangesOnBackgroundSequence(
      base::OnceCallback<base::WeakPtr<syncer::SyncableService>()>
          syncable_service_provider,
      const syncer::SyncChangeList& change_list) {
    DCHECK(GetBackendTaskRunner()->RunsTasksInCurrentSequence());

    base::WeakPtr<syncer::SyncableService> syncable_service =
        std::move(syncable_service_provider).Run();
    DCHECK(syncable_service.get());
    EXPECT_FALSE(
        syncable_service->ProcessSyncChanges(FROM_HERE, change_list).IsSet());
  }

  void SendChanges(const syncer::SyncChangeList& change_list) {
    base::RunLoop().RunUntilIdle();

    base::RunLoop loop;
    GetBackendTaskRunner()->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&SendChangesOnBackgroundSequence,
                       settings_sync_util::GetSyncableServiceProvider(
                           profile(), kModelType),
                       change_list),
        loop.QuitClosure());
    loop.Run();
  }

  void SetPolicies(const base::DictionaryValue& policies) {
    std::unique_ptr<policy::PolicyBundle> bundle(new policy::PolicyBundle());
    policy::PolicyMap& policy_map = bundle->Get(policy::PolicyNamespace(
        policy::POLICY_DOMAIN_EXTENSIONS, kManagedStorageExtensionId));
    policy_map.LoadFrom(&policies, policy::POLICY_LEVEL_MANDATORY,
                        policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD);
    policy_provider_.UpdatePolicy(std::move(bundle));
  }

 private:
  const Extension* MaybeLoadAndReplyWhenSatisfied(
      Namespace settings_namespace,
      const std::string& normal_action,
      const std::string& incognito_action,
      // May be NULL to imply not loading the extension.
      const std::string* extension_dir,
      bool is_final_action) {
    ExtensionTestMessageListener listener("waiting", true);
    ExtensionTestMessageListener listener_incognito("waiting_incognito", true);

    // Only load the extension after the listeners have been set up, to avoid
    // initialisation race conditions.
    const Extension* extension = NULL;
    if (extension_dir) {
      extension = LoadExtensionIncognito(
          test_data_dir_.AppendASCII("settings").AppendASCII(*extension_dir));
      EXPECT_TRUE(extension);
    }

    EXPECT_TRUE(listener.WaitUntilSatisfied());
    EXPECT_TRUE(listener_incognito.WaitUntilSatisfied());

    listener.Reply(
        CreateMessage(settings_namespace, normal_action, is_final_action));
    listener_incognito.Reply(
        CreateMessage(settings_namespace, incognito_action, is_final_action));
    return extension;
  }

  std::string CreateMessage(
      Namespace settings_namespace,
      const std::string& action,
      bool is_final_action) {
    base::DictionaryValue message;
    message.SetString("namespace", ToString(settings_namespace));
    message.SetString("action", action);
    message.SetBoolean("isFinalAction", is_final_action);
    std::string message_json;
    base::JSONWriter::Write(message, &message_json);
    return message_json;
  }

  void SendChangesToSyncableService(
      const syncer::SyncChangeList& change_list,
      syncer::SyncableService* settings_service) {
  }

 protected:
  policy::MockConfigurationPolicyProvider policy_provider_;
};

IN_PROC_BROWSER_TEST_F(ExtensionSettingsApiTest, SimpleTest) {
  ASSERT_TRUE(RunExtensionTest("settings/simple_test")) << message_;
}

// Structure of this test taken from IncognitoSplitMode.
// Note that only split-mode incognito is tested, because spanning mode
// incognito looks the same as normal mode when the only API activity comes
// from background pages.
IN_PROC_BROWSER_TEST_F(ExtensionSettingsApiTest, SplitModeIncognito) {
  // We need 2 ResultCatchers because we'll be running the same test in both
  // regular and incognito mode.
  ResultCatcher catcher, catcher_incognito;
  catcher.RestrictToBrowserContext(browser()->profile());
  catcher_incognito.RestrictToBrowserContext(
      browser()->profile()->GetOffTheRecordProfile());

  LoadAndReplyWhenSatisfied(SYNC,
      "assertEmpty", "assertEmpty", "split_incognito");
  ReplyWhenSatisfied(SYNC, "noop", "setFoo");
  ReplyWhenSatisfied(SYNC, "assertFoo", "assertFoo");
  ReplyWhenSatisfied(SYNC, "clear", "noop");
  ReplyWhenSatisfied(SYNC, "assertEmpty", "assertEmpty");
  ReplyWhenSatisfied(SYNC, "setFoo", "noop");
  ReplyWhenSatisfied(SYNC, "assertFoo", "assertFoo");
  ReplyWhenSatisfied(SYNC, "noop", "removeFoo");
  FinalReplyWhenSatisfied(SYNC, "assertEmpty", "assertEmpty");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  EXPECT_TRUE(catcher_incognito.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(ExtensionSettingsApiTest,
    OnChangedNotificationsBetweenBackgroundPages) {
  // We need 2 ResultCatchers because we'll be running the same test in both
  // regular and incognito mode.
  ResultCatcher catcher, catcher_incognito;
  catcher.RestrictToBrowserContext(browser()->profile());
  catcher_incognito.RestrictToBrowserContext(
      browser()->profile()->GetOffTheRecordProfile());

  LoadAndReplyWhenSatisfied(SYNC,
      "assertNoNotifications", "assertNoNotifications", "split_incognito");
  ReplyWhenSatisfied(SYNC, "noop", "setFoo");
  ReplyWhenSatisfied(SYNC,
      "assertAddFooNotification", "assertAddFooNotification");
  ReplyWhenSatisfied(SYNC, "clearNotifications", "clearNotifications");
  ReplyWhenSatisfied(SYNC, "removeFoo", "noop");
  FinalReplyWhenSatisfied(SYNC,
      "assertDeleteFooNotification", "assertDeleteFooNotification");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  EXPECT_TRUE(catcher_incognito.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(ExtensionSettingsApiTest,
    SyncAndLocalAreasAreSeparate) {
  // We need 2 ResultCatchers because we'll be running the same test in both
  // regular and incognito mode.
  ResultCatcher catcher, catcher_incognito;
  catcher.RestrictToBrowserContext(browser()->profile());
  catcher_incognito.RestrictToBrowserContext(
      browser()->profile()->GetOffTheRecordProfile());

  LoadAndReplyWhenSatisfied(SYNC,
      "assertNoNotifications", "assertNoNotifications", "split_incognito");

  ReplyWhenSatisfied(SYNC, "noop", "setFoo");
  ReplyWhenSatisfied(SYNC, "assertFoo", "assertFoo");
  ReplyWhenSatisfied(SYNC,
      "assertAddFooNotification", "assertAddFooNotification");
  ReplyWhenSatisfied(LOCAL, "assertEmpty", "assertEmpty");
  ReplyWhenSatisfied(LOCAL, "assertNoNotifications", "assertNoNotifications");

  ReplyWhenSatisfied(SYNC, "clearNotifications", "clearNotifications");

  ReplyWhenSatisfied(LOCAL, "setFoo", "noop");
  ReplyWhenSatisfied(LOCAL, "assertFoo", "assertFoo");
  ReplyWhenSatisfied(LOCAL,
      "assertAddFooNotification", "assertAddFooNotification");
  ReplyWhenSatisfied(SYNC, "assertFoo", "assertFoo");
  ReplyWhenSatisfied(SYNC, "assertNoNotifications", "assertNoNotifications");

  ReplyWhenSatisfied(LOCAL, "clearNotifications", "clearNotifications");

  ReplyWhenSatisfied(LOCAL, "noop", "removeFoo");
  ReplyWhenSatisfied(LOCAL, "assertEmpty", "assertEmpty");
  ReplyWhenSatisfied(LOCAL,
      "assertDeleteFooNotification", "assertDeleteFooNotification");
  ReplyWhenSatisfied(SYNC, "assertFoo", "assertFoo");
  ReplyWhenSatisfied(SYNC, "assertNoNotifications", "assertNoNotifications");

  ReplyWhenSatisfied(LOCAL, "clearNotifications", "clearNotifications");

  ReplyWhenSatisfied(SYNC, "removeFoo", "noop");
  ReplyWhenSatisfied(SYNC, "assertEmpty", "assertEmpty");
  ReplyWhenSatisfied(SYNC,
      "assertDeleteFooNotification", "assertDeleteFooNotification");
  ReplyWhenSatisfied(LOCAL, "assertNoNotifications", "assertNoNotifications");
  FinalReplyWhenSatisfied(LOCAL, "assertEmpty", "assertEmpty");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  EXPECT_TRUE(catcher_incognito.GetNextResult()) << catcher.message();
}

// Disabled, see crbug.com/101110
IN_PROC_BROWSER_TEST_F(ExtensionSettingsApiTest,
    DISABLED_OnChangedNotificationsFromSync) {
  // We need 2 ResultCatchers because we'll be running the same test in both
  // regular and incognito mode.
  ResultCatcher catcher, catcher_incognito;
  catcher.RestrictToBrowserContext(browser()->profile());
  catcher_incognito.RestrictToBrowserContext(
      browser()->profile()->GetOffTheRecordProfile());

  const Extension* extension =
      LoadAndReplyWhenSatisfied(SYNC,
          "assertNoNotifications", "assertNoNotifications", "split_incognito");
  const std::string& extension_id = extension->id();

  syncer::FakeSyncChangeProcessor sync_processor;
  InitSync(&sync_processor);

  // Set "foo" to "bar" via sync.
  syncer::SyncChangeList sync_changes;
  base::Value bar("bar");
  sync_changes.push_back(settings_sync_util::CreateAdd(
      extension_id, "foo", bar, kModelType));
  SendChanges(sync_changes);

  ReplyWhenSatisfied(SYNC,
      "assertAddFooNotification", "assertAddFooNotification");
  ReplyWhenSatisfied(SYNC, "clearNotifications", "clearNotifications");

  // Remove "foo" via sync.
  sync_changes.clear();
  sync_changes.push_back(settings_sync_util::CreateDelete(
      extension_id, "foo", kModelType));
  SendChanges(sync_changes);

  FinalReplyWhenSatisfied(SYNC,
      "assertDeleteFooNotification", "assertDeleteFooNotification");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  EXPECT_TRUE(catcher_incognito.GetNextResult()) << catcher.message();
}

// Disabled, see crbug.com/101110
//
// TODO: boring test, already done in the unit tests.  What we really should be
// be testing is that the areas don't overlap.
IN_PROC_BROWSER_TEST_F(ExtensionSettingsApiTest,
    DISABLED_OnChangedNotificationsFromSyncNotSentToLocal) {
  // We need 2 ResultCatchers because we'll be running the same test in both
  // regular and incognito mode.
  ResultCatcher catcher, catcher_incognito;
  catcher.RestrictToBrowserContext(browser()->profile());
  catcher_incognito.RestrictToBrowserContext(
      browser()->profile()->GetOffTheRecordProfile());

  const Extension* extension =
      LoadAndReplyWhenSatisfied(LOCAL,
          "assertNoNotifications", "assertNoNotifications", "split_incognito");
  const std::string& extension_id = extension->id();

  syncer::FakeSyncChangeProcessor sync_processor;
  InitSync(&sync_processor);

  // Set "foo" to "bar" via sync.
  syncer::SyncChangeList sync_changes;
  base::Value bar("bar");
  sync_changes.push_back(settings_sync_util::CreateAdd(
      extension_id, "foo", bar, kModelType));
  SendChanges(sync_changes);

  ReplyWhenSatisfied(LOCAL, "assertNoNotifications", "assertNoNotifications");

  // Remove "foo" via sync.
  sync_changes.clear();
  sync_changes.push_back(settings_sync_util::CreateDelete(
      extension_id, "foo", kModelType));
  SendChanges(sync_changes);

  FinalReplyWhenSatisfied(LOCAL,
      "assertNoNotifications", "assertNoNotifications");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  EXPECT_TRUE(catcher_incognito.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(ExtensionSettingsApiTest, IsStorageEnabled) {
  StorageFrontend* frontend = StorageFrontend::Get(browser()->profile());
  EXPECT_TRUE(frontend->IsStorageEnabled(LOCAL));
  EXPECT_TRUE(frontend->IsStorageEnabled(SYNC));

  EXPECT_TRUE(frontend->IsStorageEnabled(MANAGED));
}

IN_PROC_BROWSER_TEST_F(ExtensionSettingsApiTest, ExtensionsSchemas) {
  // Verifies that the Schemas for the extensions domain are created on startup.
  Profile* profile = browser()->profile();
  ExtensionSystem* extension_system = ExtensionSystem::Get(profile);
  if (!extension_system->ready().is_signaled()) {
    // Wait until the extension system is ready.
    base::RunLoop run_loop;
    extension_system->ready().Post(FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
    ASSERT_TRUE(extension_system->ready().is_signaled());
  }

  // This test starts without any test extensions installed.
  EXPECT_FALSE(GetSingleLoadedExtension());
  message_.clear();

  policy::SchemaRegistry* registry =
      profile->GetPolicySchemaRegistryService()->registry();
  ASSERT_TRUE(registry);
  EXPECT_FALSE(registry->schema_map()->GetSchema(policy::PolicyNamespace(
      policy::POLICY_DOMAIN_EXTENSIONS, kManagedStorageExtensionId)));

  MockSchemaRegistryObserver observer;
  registry->AddObserver(&observer);

  // Install a managed extension.
  EXPECT_CALL(observer, OnSchemaRegistryUpdated(true));
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("settings/managed_storage"));
  ASSERT_TRUE(extension);
  Mock::VerifyAndClearExpectations(&observer);
  registry->RemoveObserver(&observer);

  // Verify that its schema has been published, and verify its contents.
  const policy::Schema* schema =
      registry->schema_map()->GetSchema(policy::PolicyNamespace(
          policy::POLICY_DOMAIN_EXTENSIONS, kManagedStorageExtensionId));
  ASSERT_TRUE(schema);

  ASSERT_TRUE(schema->valid());
  ASSERT_EQ(base::Value::Type::DICTIONARY, schema->type());
  ASSERT_TRUE(schema->GetKnownProperty("string-policy").valid());
  EXPECT_EQ(base::Value::Type::STRING,
            schema->GetKnownProperty("string-policy").type());
  ASSERT_TRUE(schema->GetKnownProperty("int-policy").valid());
  EXPECT_EQ(base::Value::Type::INTEGER,
            schema->GetKnownProperty("int-policy").type());
  ASSERT_TRUE(schema->GetKnownProperty("double-policy").valid());
  EXPECT_EQ(base::Value::Type::DOUBLE,
            schema->GetKnownProperty("double-policy").type());
  ASSERT_TRUE(schema->GetKnownProperty("boolean-policy").valid());
  EXPECT_EQ(base::Value::Type::BOOLEAN,
            schema->GetKnownProperty("boolean-policy").type());

  policy::Schema list = schema->GetKnownProperty("list-policy");
  ASSERT_TRUE(list.valid());
  ASSERT_EQ(base::Value::Type::LIST, list.type());
  ASSERT_TRUE(list.GetItems().valid());
  EXPECT_EQ(base::Value::Type::STRING, list.GetItems().type());

  policy::Schema dict = schema->GetKnownProperty("dict-policy");
  ASSERT_TRUE(dict.valid());
  ASSERT_EQ(base::Value::Type::DICTIONARY, dict.type());
  list = dict.GetKnownProperty("list");
  ASSERT_TRUE(list.valid());
  ASSERT_EQ(base::Value::Type::LIST, list.type());
  dict = list.GetItems();
  ASSERT_TRUE(dict.valid());
  ASSERT_EQ(base::Value::Type::DICTIONARY, dict.type());
  ASSERT_TRUE(dict.GetProperty("anything").valid());
  EXPECT_EQ(base::Value::Type::INTEGER, dict.GetProperty("anything").type());
}

IN_PROC_BROWSER_TEST_F(ExtensionSettingsApiTest, ManagedStorage) {
  // Set policies for the test extension.
  std::unique_ptr<base::DictionaryValue> policy =
      extensions::DictionaryBuilder()
          .Set("string-policy", "value")
          .Set("int-policy", -123)
          .Set("double-policy", 456e7)
          .Set("boolean-policy", true)
          .Set("list-policy", extensions::ListBuilder()
                                  .Append("one")
                                  .Append("two")
                                  .Append("three")
                                  .Build())
          .Set("dict-policy",
               extensions::DictionaryBuilder()
                   .Set("list", extensions::ListBuilder()
                                    .Append(extensions::DictionaryBuilder()
                                                .Set("one", 1)
                                                .Set("two", 2)
                                                .Build())
                                    .Append(extensions::DictionaryBuilder()
                                                .Set("three", 3)
                                                .Build())
                                    .Build())
                   .Build())
          .Build();
  SetPolicies(*policy);
  // Now run the extension.
  ASSERT_TRUE(RunExtensionTest("settings/managed_storage")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionSettingsApiTest,
                       DISABLED_PRE_ManagedStorageEvents) {
  ResultCatcher catcher;

  // This test starts without any test extensions installed.
  EXPECT_FALSE(GetSingleLoadedExtension());
  message_.clear();

  // Set policies for the test extension.
  std::unique_ptr<base::DictionaryValue> policy =
      extensions::DictionaryBuilder()
          .Set("constant-policy", "aaa")
          .Set("changes-policy", "bbb")
          .Set("deleted-policy", "ccc")
          .Build();
  SetPolicies(*policy);

  ExtensionTestMessageListener ready_listener("ready", false);
  // Load the extension to install the event listener.
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("settings/managed_storage_events"));
  ASSERT_TRUE(extension);
  // Wait until the extension sends the "ready" message.
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());

  // Now change the policies and wait until the extension is done.
  policy = extensions::DictionaryBuilder()
      .Set("constant-policy", "aaa")
      .Set("changes-policy", "ddd")
      .Set("new-policy", "eee")
      .Build();
  SetPolicies(*policy);
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(ExtensionSettingsApiTest,
                       DISABLED_ManagedStorageEvents) {
  // This test runs after PRE_ManagedStorageEvents without having deleted the
  // profile, so the extension is still around. While the browser restarted the
  // policy went back to the empty default, and so the extension should receive
  // the corresponding change events.

  ResultCatcher catcher;

  // Verify that the test extension is still installed.
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension);
  EXPECT_EQ(kManagedStorageExtensionId, extension->id());

  // Running the test again skips the onInstalled callback, and just triggers
  // the onChanged notification.
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(ExtensionSettingsApiTest, ManagedStorageDisabled) {
  // Disable the 'managed' namespace.
  StorageFrontend* frontend = StorageFrontend::Get(browser()->profile());
  frontend->DisableStorageForTesting(MANAGED);
  EXPECT_FALSE(frontend->IsStorageEnabled(MANAGED));
  // Now run the extension.
  ASSERT_TRUE(RunExtensionTest("settings/managed_storage_disabled"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionSettingsApiTest, StorageAreaOnChanged) {
  ASSERT_TRUE(RunExtensionTest("settings/storage_area")) << message_;
}

}  // namespace extensions
