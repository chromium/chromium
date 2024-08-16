// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "build/build_config.h"
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
#include "components/sync/model/sync_change.h"
#include "components/sync/model/sync_change_processor.h"
#include "components/sync/model/syncable_service.h"
#include "components/sync/test/fake_sync_change_processor.h"
#include "components/sync/test/sync_change_processor_wrapper_for_test.h"
#include "components/version_info/channel.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api/storage/backend_task_runner.h"
#include "extensions/browser/api/storage/settings_namespace.h"
#include "extensions/browser/api/storage/storage_area_namespace.h"
#include "extensions/browser/api/storage/storage_frontend.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace extensions {

using testing::NiceMock;

namespace {

// TODO(kalman): test both EXTENSION_SETTINGS and APP_SETTINGS.
const syncer::DataType kDataType = syncer::EXTENSION_SETTINGS;

// The managed_storage extension has a key defined in its manifest, so that
// its extension ID is well-known and the policy system can push policies for
// the extension.
const char kManagedStorageExtensionId[] = "kjmkgkdkpedkejedfhmfcenooemhbpbo";

class TestSchemaRegistryObserver : public policy::SchemaRegistry::Observer {
 public:
  TestSchemaRegistryObserver() = default;
  ~TestSchemaRegistryObserver() override = default;
  TestSchemaRegistryObserver(const TestSchemaRegistryObserver&) = delete;
  TestSchemaRegistryObserver& operator=(const TestSchemaRegistryObserver&) =
      delete;

  void OnSchemaRegistryUpdated(bool has_new_schemas) override {
    has_new_schemas_ = has_new_schemas;
    run_loop_.Quit();
  }

  void WaitForSchemaRegistryUpdated() { run_loop_.Run(); }

  bool has_new_schemas() const { return has_new_schemas_; }

 private:
  bool has_new_schemas_ = false;
  base::RunLoop run_loop_;
};

}  // namespace

class ExtensionSettingsApiTest : public ExtensionApiTest {
 public:
  explicit ExtensionSettingsApiTest(
      ContextType context_type = ContextType::kNone)
      : ExtensionApiTest(context_type) {}
  ~ExtensionSettingsApiTest() override = default;
  ExtensionSettingsApiTest(const ExtensionSettingsApiTest&) = delete;
  ExtensionSettingsApiTest& operator=(const ExtensionSettingsApiTest&) = delete;

 protected:
  void SetUpInProcessBrowserTestFixture() override {
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();

    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy_provider_.SetAutoRefresh();
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
  }

  void ReplyWhenSatisfied(StorageAreaNamespace storage_area,
                          const std::string& normal_action,
                          const std::string& incognito_action) {
    MaybeLoadAndReplyWhenSatisfied(storage_area, normal_action,
                                   incognito_action, nullptr, false);
  }

  const Extension* LoadAndReplyWhenSatisfied(
      StorageAreaNamespace storage_area,
      const std::string& normal_action,
      const std::string& incognito_action,
      const std::string& extension_dir) {
    return MaybeLoadAndReplyWhenSatisfied(
        storage_area, normal_action, incognito_action, &extension_dir, false);
  }

  void FinalReplyWhenSatisfied(StorageAreaNamespace storage_area,
                               const std::string& normal_action,
                               const std::string& incognito_action) {
    MaybeLoadAndReplyWhenSatisfied(storage_area, normal_action,
                                   incognito_action, nullptr, true);
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
                kDataType, syncer::SyncDataList(),
                std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
                    sync_processor))
            .has_value());
  }

  void InitSync(syncer::SyncChangeProcessor* sync_processor) {
    base::RunLoop().RunUntilIdle();

    base::RunLoop loop;
    GetBackendTaskRunner()->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&InitSyncOnBackgroundSequence,
                       settings_sync_util::GetSyncableServiceProvider(
                           profile(), kDataType),
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
    EXPECT_FALSE(syncable_service->ProcessSyncChanges(FROM_HERE, change_list)
                     .has_value());
  }

  void SendChanges(const syncer::SyncChangeList& change_list) {
    base::RunLoop().RunUntilIdle();

    base::RunLoop loop;
    GetBackendTaskRunner()->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&SendChangesOnBackgroundSequence,
                       settings_sync_util::GetSyncableServiceProvider(
                           profile(), kDataType),
                       change_list),
        loop.QuitClosure());
    loop.Run();
  }

  void SetPolicies(const base::Value::Dict& policies) {
    policy::PolicyBundle bundle;
    policy::PolicyMap& policy_map = bundle.Get(policy::PolicyNamespace(
        policy::POLICY_DOMAIN_EXTENSIONS, kManagedStorageExtensionId));
    policy_map.LoadFrom(policies, policy::POLICY_LEVEL_MANDATORY,
                        policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD);
    policy_provider_.UpdatePolicy(std::move(bundle));
  }

 private:
  const Extension* MaybeLoadAndReplyWhenSatisfied(
      StorageAreaNamespace storage_area,
      const std::string& normal_action,
      const std::string& incognito_action,
      // May be NULL to imply not loading the extension.
      const std::string* extension_dir,
      bool is_final_action) {
    ExtensionTestMessageListener listener("waiting", ReplyBehavior::kWillReply);
    ExtensionTestMessageListener listener_incognito("waiting_incognito",
                                                    ReplyBehavior::kWillReply);

    // Only load the extension after the listeners have been set up, to avoid
    // initialisation race conditions.
    const Extension* extension = nullptr;
    if (extension_dir) {
      extension = LoadExtension(
          test_data_dir_.AppendASCII("settings").AppendASCII(*extension_dir),
          {.allow_in_incognito = true});
      EXPECT_TRUE(extension);
    }

    EXPECT_TRUE(listener.WaitUntilSatisfied());
    EXPECT_TRUE(listener_incognito.WaitUntilSatisfied());

    listener.Reply(CreateMessage(storage_area, normal_action, is_final_action));
    listener_incognito.Reply(
        CreateMessage(storage_area, incognito_action, is_final_action));
    return extension;
  }

  std::string CreateMessage(StorageAreaNamespace storage_area,
                            const std::string& action,
                            bool is_final_action) {
    base::Value::Dict message;
    message.Set("namespace", StorageAreaToString(storage_area));
    message.Set("action", action);
    message.Set("isFinalAction", is_final_action);
    std::string message_json;
    base::JSONWriter::Write(message, &message_json);
    return message_json;
  }

  void SendChangesToSyncableService(
      const syncer::SyncChangeList& change_list,
      syncer::SyncableService* settings_service) {
  }

 protected:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
};

IN_PROC_BROWSER_TEST_F(ExtensionSettingsApiTest,
                       SessionInUnsupportedExtension) {
  constexpr char kManifest[] =
      R"({
      "name": "Unsupported manifest version for Storage API",
      "manifest_version": 2,
      "version": "0.1",
      "background": {"scripts": ["script.js"]},
      "permissions": ["storage"]
    })";
  constexpr char kScript[] =
      R"({
      chrome.test.runTests([
        function unsupported() {
          chrome.test.assertEq(undefined, chrome.storage.session),
          chrome.test.assertTrue(!!chrome.storage.local);
          chrome.test.succeed();
        }
      ])
    })";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("script.js"), kScript);

  ResultCatcher catcher;
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

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
  ResultCatcher catcher;
  ResultCatcher catcher_incognito;
  catcher.RestrictToBrowserContext(browser()->profile());
  catcher_incognito.RestrictToBrowserContext(
      browser()->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true));

  // Sync, local and managed follow the same storage flow (RunWithStorage),
  // whereas session follows a separate flow (RunWithSession). For the purpose
  // of this test we can just test sync and session.
  StorageAreaNamespace storage_areas[2] = {StorageAreaNamespace::kSync,
                                           StorageAreaNamespace::kSession};
  LoadAndReplyWhenSatisfied(StorageAreaNamespace::kSync, "assertEmpty",
                            "assertEmpty", "split_incognito");
  for (const StorageAreaNamespace& storage_area : storage_areas) {
    ReplyWhenSatisfied(storage_area, "assertEmpty", "assertEmpty");
    ReplyWhenSatisfied(storage_area, "noop", "setFoo");
    ReplyWhenSatisfied(storage_area, "assertFoo", "assertFoo");
    ReplyWhenSatisfied(storage_area, "clear", "noop");
    ReplyWhenSatisfied(storage_area, "assertEmpty", "assertEmpty");
    ReplyWhenSatisfied(storage_area, "setFoo", "noop");
    ReplyWhenSatisfied(storage_area, "assertFoo", "assertFoo");
    ReplyWhenSatisfied(storage_area, "noop", "removeFoo");
    ReplyWhenSatisfied(storage_area, "assertEmpty", "assertEmpty");
  }
  FinalReplyWhenSatisfied(StorageAreaNamespace::kSession, "assertEmpty",
                          "assertEmpty");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  EXPECT_TRUE(catcher_incognito.GetNextResult()) << catcher.message();
}

// TODO(crbug.com/40189896): Service worker extension listener should receive an
// event before the callback is made. Current workaround: wait for the event to
// be received by the extension before checking for it. Potential solution: once
// browser-side observation of SW lifetime work is finished, check if it fixes
// this test.
IN_PROC_BROWSER_TEST_F(ExtensionSettingsApiTest,
                       OnChangedNotificationsBetweenBackgroundPages) {
  // We need 2 ResultCatchers because we'll be running the same test in both
  // regular and incognito mode.
  ResultCatcher catcher;
  ResultCatcher catcher_incognito;
  catcher.RestrictToBrowserContext(browser()->profile());
  catcher_incognito.RestrictToBrowserContext(
      browser()->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true));

  StorageAreaNamespace storage_areas[2] = {StorageAreaNamespace::kSync,
                                           StorageAreaNamespace::kSession};

  for (const StorageAreaNamespace& storage_area : storage_areas) {
    // We need to load the extension when it's the first reply.
    // kSync is the first storage area to run.
    if (storage_area == StorageAreaNamespace::kSync) {
      LoadAndReplyWhenSatisfied(StorageAreaNamespace::kSync,
                                "assertNoNotifications",
                                "assertNoNotifications", "split_incognito");
    } else {
      ReplyWhenSatisfied(storage_area, "assertNoNotifications",
                         "assertNoNotifications");
    }

    ReplyWhenSatisfied(storage_area, "noop", "setFoo");
    ReplyWhenSatisfied(storage_area, "assertAddFooNotification",
                       "assertAddFooNotification");
    ReplyWhenSatisfied(storage_area, "clearNotifications",
                       "clearNotifications");
    ReplyWhenSatisfied(storage_area, "removeFoo", "noop");

    // We need to end the test with a final reply when it's the last reply.
    // kSession is the last storage area to run.
    if (storage_area == StorageAreaNamespace::kSession) {
      FinalReplyWhenSatisfied(StorageAreaNamespace::kSession,
                              "assertDeleteFooNotification",
                              "assertDeleteFooNotification");
    } else {
      ReplyWhenSatisfied(storage_area, "assertDeleteFooNotification",
                         "assertDeleteFooNotification");
    }
  }

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  EXPECT_TRUE(catcher_incognito.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(ExtensionSettingsApiTest,
                       SyncLocalAndSessionAreasAreSeparate) {
  // We need 2 ResultCatchers because we'll be running the same test in both
  // regular and incognito mode.
  ResultCatcher catcher;
  ResultCatcher catcher_incognito;
  catcher.RestrictToBrowserContext(browser()->profile());
  catcher_incognito.RestrictToBrowserContext(
      browser()->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true));

  LoadAndReplyWhenSatisfied(StorageAreaNamespace::kSync,
                            "assertNoNotifications", "assertNoNotifications",
                            "split_incognito");

  ReplyWhenSatisfied(StorageAreaNamespace::kSync, "noop", "setFoo");
  ReplyWhenSatisfied(StorageAreaNamespace::kSync, "assertFoo", "assertFoo");
  ReplyWhenSatisfied(StorageAreaNamespace::kSync, "assertAddFooNotification",
                     "assertAddFooNotification");
  ReplyWhenSatisfied(StorageAreaNamespace::kLocal, "assertEmpty",
                     "assertEmpty");
  ReplyWhenSatisfied(StorageAreaNamespace::kLocal, "assertNoNotifications",
                     "assertNoNotifications");
  ReplyWhenSatisfied(StorageAreaNamespace::kSession, "assertEmpty",
                     "assertEmpty");
  ReplyWhenSatisfied(StorageAreaNamespace::kSession, "assertNoNotifications",
                     "assertNoNotifications");

  ReplyWhenSatisfied(StorageAreaNamespace::kSync, "clearNotifications",
                     "clearNotifications");

  ReplyWhenSatisfied(StorageAreaNamespace::kLocal, "setFoo", "noop");
  ReplyWhenSatisfied(StorageAreaNamespace::kLocal, "assertFoo", "assertFoo");
  ReplyWhenSatisfied(StorageAreaNamespace::kLocal, "assertAddFooNotification",
                     "assertAddFooNotification");
  ReplyWhenSatisfied(StorageAreaNamespace::kSync, "assertFoo", "assertFoo");
  ReplyWhenSatisfied(StorageAreaNamespace::kSync, "assertNoNotifications",
                     "assertNoNotifications");
  ReplyWhenSatisfied(StorageAreaNamespace::kSession, "assertEmpty",
                     "assertEmpty");
  ReplyWhenSatisfied(StorageAreaNamespace::kSession, "assertNoNotifications",
                     "assertNoNotifications");

  ReplyWhenSatisfied(StorageAreaNamespace::kLocal, "clearNotifications",
                     "clearNotifications");

  ReplyWhenSatisfied(StorageAreaNamespace::kSession, "setFoo", "noop");
  ReplyWhenSatisfied(StorageAreaNamespace::kSession, "assertFoo", "assertFoo");
  ReplyWhenSatisfied(StorageAreaNamespace::kSession, "assertAddFooNotification",
                     "assertAddFooNotification");
  ReplyWhenSatisfied(StorageAreaNamespace::kSync, "assertFoo", "assertFoo");
  ReplyWhenSatisfied(StorageAreaNamespace::kSync, "assertNoNotifications",
                     "assertNoNotifications");
  ReplyWhenSatisfied(StorageAreaNamespace::kLocal, "assertFoo", "assertFoo");
  ReplyWhenSatisfied(StorageAreaNamespace::kLocal, "assertNoNotifications",
                     "assertNoNotifications");

  ReplyWhenSatisfied(StorageAreaNamespace::kSession, "clearNotifications",
                     "clearNotifications");

  ReplyWhenSatisfied(StorageAreaNamespace::kLocal, "noop", "removeFoo");
  ReplyWhenSatisfied(StorageAreaNamespace::kLocal, "assertEmpty",
                     "assertEmpty");
  ReplyWhenSatisfied(StorageAreaNamespace::kLocal,
                     "assertDeleteFooNotification",
                     "assertDeleteFooNotification");
  ReplyWhenSatisfied(StorageAreaNamespace::kSync, "assertFoo", "assertFoo");
  ReplyWhenSatisfied(StorageAreaNamespace::kSync, "assertNoNotifications",
                     "assertNoNotifications");
  ReplyWhenSatisfied(StorageAreaNamespace::kSession, "assertFoo", "assertFoo");
  ReplyWhenSatisfied(StorageAreaNamespace::kSession, "assertNoNotifications",
                     "assertNoNotifications");

  ReplyWhenSatisfied(StorageAreaNamespace::kLocal, "clearNotifications",
                     "clearNotifications");

  ReplyWhenSatisfied(StorageAreaNamespace::kSync, "removeFoo", "noop");
  ReplyWhenSatisfied(StorageAreaNamespace::kSync, "assertEmpty", "assertEmpty");
  ReplyWhenSatisfied(StorageAreaNamespace::kSync, "assertDeleteFooNotification",
                     "assertDeleteFooNotification");
  ReplyWhenSatisfied(StorageAreaNamespace::kLocal, "assertEmpty",
                     "assertEmpty");
  ReplyWhenSatisfied(StorageAreaNamespace::kLocal, "assertNoNotifications",
                     "assertNoNotifications");
  ReplyWhenSatisfied(StorageAreaNamespace::kSession, "assertFoo", "assertFoo");
  ReplyWhenSatisfied(StorageAreaNamespace::kSession, "assertNoNotifications",
                     "assertNoNotifications");

  ReplyWhenSatisfied(StorageAreaNamespace::kSync, "clearNotifications",
                     "clearNotifications");

  ReplyWhenSatisfied(StorageAreaNamespace::kSession, "removeFoo", "noop");
  ReplyWhenSatisfied(StorageAreaNamespace::kSession, "assertEmpty",
                     "assertEmpty");
  ReplyWhenSatisfied(StorageAreaNamespace::kSession,
                     "assertDeleteFooNotification",
                     "assertDeleteFooNotification");
  ReplyWhenSatisfied(StorageAreaNamespace::kSync, "assertEmpty", "assertEmpty");
  ReplyWhenSatisfied(StorageAreaNamespace::kSync, "assertNoNotifications",
                     "assertNoNotifications");
  ReplyWhenSatisfied(StorageAreaNamespace::kLocal, "assertEmpty",
                     "assertEmpty");
  FinalReplyWhenSatisfied(StorageAreaNamespace::kLocal, "assertNoNotifications",
                          "assertNoNotifications");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  EXPECT_TRUE(catcher_incognito.GetNextResult()) << catcher.message();
}

// TODO(crbug.com/40189896): Service worker extension listener should receive an
// event before the callback is made. Current workaround: wait for the event to
// be received by the extension before checking for it. Potential solution: once
// browser-side observation of SW lifetime work is finished, check if it fixes
// this test.
IN_PROC_BROWSER_TEST_F(ExtensionSettingsApiTest,
                       OnChangedNotificationsFromSync) {
  // We need 2 ResultCatchers because we'll be running the same test in both
  // regular and incognito mode.
  ResultCatcher catcher, catcher_incognito;
  catcher.RestrictToBrowserContext(browser()->profile());
  catcher_incognito.RestrictToBrowserContext(
      browser()->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true));

  const Extension* extension = LoadAndReplyWhenSatisfied(
      StorageAreaNamespace::kSync, "assertNoNotifications",
      "assertNoNotifications", "split_incognito");
  const ExtensionId& extension_id = extension->id();

  syncer::FakeSyncChangeProcessor sync_processor;
  InitSync(&sync_processor);

  // Set "foo" to "bar" via sync.
  syncer::SyncChangeList sync_changes;
  base::Value bar("bar");
  sync_changes.push_back(
      settings_sync_util::CreateAdd(extension_id, "foo", bar, kDataType));
  SendChanges(sync_changes);

  ReplyWhenSatisfied(StorageAreaNamespace::kSync, "assertAddFooNotification",
                     "assertAddFooNotification");
  ReplyWhenSatisfied(StorageAreaNamespace::kSync, "clearNotifications",
                     "clearNotifications");

  // Remove "foo" via sync.
  sync_changes.clear();
  sync_changes.push_back(
      settings_sync_util::CreateDelete(extension_id, "foo", kDataType));
  SendChanges(sync_changes);

  FinalReplyWhenSatisfied(StorageAreaNamespace::kSync,
                          "assertDeleteFooNotification",
                          "assertDeleteFooNotification");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  EXPECT_TRUE(catcher_incognito.GetNextResult()) << catcher.message();
}

// TODO: boring test, already done in the unit tests.  What we really should be
// be testing is that the areas don't overlap.
IN_PROC_BROWSER_TEST_F(ExtensionSettingsApiTest,
                       OnChangedNotificationsFromSyncNotSentToLocal) {
  // We need 2 ResultCatchers because we'll be running the same test in both
  // regular and incognito mode.
  ResultCatcher catcher, catcher_incognito;
  catcher.RestrictToBrowserContext(browser()->profile());
  catcher_incognito.RestrictToBrowserContext(
      browser()->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true));

  const Extension* extension = LoadAndReplyWhenSatisfied(
      StorageAreaNamespace::kLocal, "assertNoNotifications",
      "assertNoNotifications", "split_incognito");
  const ExtensionId& extension_id = extension->id();

  syncer::FakeSyncChangeProcessor sync_processor;
  InitSync(&sync_processor);

  // Set "foo" to "bar" via sync.
  syncer::SyncChangeList sync_changes;
  base::Value bar("bar");
  sync_changes.push_back(
      settings_sync_util::CreateAdd(extension_id, "foo", bar, kDataType));
  SendChanges(sync_changes);

  ReplyWhenSatisfied(StorageAreaNamespace::kLocal, "assertNoNotifications",
                     "assertNoNotifications");

  // Remove "foo" via sync.
  sync_changes.clear();
  sync_changes.push_back(
      settings_sync_util::CreateDelete(extension_id, "foo", kDataType));
  SendChanges(sync_changes);

  FinalReplyWhenSatisfied(StorageAreaNamespace::kLocal, "assertNoNotifications",
                          "assertNoNotifications");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  EXPECT_TRUE(catcher_incognito.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(ExtensionSettingsApiTest, IsStorageEnabled) {
  StorageFrontend* frontend = StorageFrontend::Get(browser()->profile());
  EXPECT_TRUE(frontend->IsStorageEnabled(settings_namespace::LOCAL));
  EXPECT_TRUE(frontend->IsStorageEnabled(settings_namespace::SYNC));

  EXPECT_TRUE(frontend->IsStorageEnabled(settings_namespace::MANAGED));
}

using ContextType = ExtensionBrowserTest::ContextType;

class ExtensionSettingsManagedStorageApiTest
    : public ExtensionSettingsApiTest,
      public testing::WithParamInterface<ContextType> {
 public:
  ExtensionSettingsManagedStorageApiTest()
      : ExtensionSettingsApiTest(GetParam()) {}
  ~ExtensionSettingsManagedStorageApiTest() override = default;
  ExtensionSettingsManagedStorageApiTest(
      const ExtensionSettingsManagedStorageApiTest& other) = delete;
  ExtensionSettingsManagedStorageApiTest& operator=(
      const ExtensionSettingsManagedStorageApiTest& other) = delete;

  // TODO(crbug.com/40789870): Remove this.
  // The ManagedStorageEvents test has a PRE_ step loads an extension which
  // then runs in the main step. Since the extension immediately starts
  // running the tests, constructing a ResultCatcher in the body of the
  // fixture will occasionally miss the result from the JS test, leading
  // to a flaky result. This ResultCatcher will be always be constructed
  // before the test starts running.
  ResultCatcher events_result_catcher_;
};

INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         ExtensionSettingsManagedStorageApiTest,
                         ::testing::Values(ContextType::kPersistentBackground));

INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         ExtensionSettingsManagedStorageApiTest,
                         ::testing::Values(ContextType::kServiceWorker));

IN_PROC_BROWSER_TEST_P(ExtensionSettingsManagedStorageApiTest,
                       ExtensionsSchemas) {
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

  TestSchemaRegistryObserver observer;
  registry->AddObserver(&observer);

  // Install a managed extension.
  ExtensionTestMessageListener listener("ready");
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("settings/managed_storage_schemas"));
  ASSERT_TRUE(listener.WaitUntilSatisfied());
  ASSERT_TRUE(extension);
  observer.WaitForSchemaRegistryUpdated();

  // Verify the schemas were installed.
  EXPECT_TRUE(observer.has_new_schemas());

  registry->RemoveObserver(&observer);

  // Verify that its schema has been published, and verify its contents.
  const policy::Schema* schema =
      registry->schema_map()->GetSchema(policy::PolicyNamespace(
          policy::POLICY_DOMAIN_EXTENSIONS, kManagedStorageExtensionId));
  ASSERT_TRUE(schema);

  ASSERT_TRUE(schema->valid());
  ASSERT_EQ(base::Value::Type::DICT, schema->type());
  ASSERT_TRUE(schema->GetKnownProperty("string-policy").valid());
  EXPECT_EQ(base::Value::Type::STRING,
            schema->GetKnownProperty("string-policy").type());
  ASSERT_TRUE(schema->GetKnownProperty("string-enum-policy").valid());
  EXPECT_EQ(base::Value::Type::STRING,
            schema->GetKnownProperty("string-enum-policy").type());
  ASSERT_TRUE(schema->GetKnownProperty("int-policy").valid());
  EXPECT_EQ(base::Value::Type::INTEGER,
            schema->GetKnownProperty("int-policy").type());
  ASSERT_TRUE(schema->GetKnownProperty("int-enum-policy").valid());
  EXPECT_EQ(base::Value::Type::INTEGER,
            schema->GetKnownProperty("int-enum-policy").type());
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
  ASSERT_EQ(base::Value::Type::DICT, dict.type());
  list = dict.GetKnownProperty("list");
  ASSERT_TRUE(list.valid());
  ASSERT_EQ(base::Value::Type::LIST, list.type());
  dict = list.GetItems();
  ASSERT_TRUE(dict.valid());
  ASSERT_EQ(base::Value::Type::DICT, dict.type());
  ASSERT_TRUE(dict.GetProperty("anything").valid());
  EXPECT_EQ(base::Value::Type::INTEGER, dict.GetProperty("anything").type());
}

// TODO(crbug.com/40789870): This test should be rewritten. See the bug for more
// details.
IN_PROC_BROWSER_TEST_P(ExtensionSettingsManagedStorageApiTest, ManagedStorage) {
  // Set policies for the test extension.
  base::Value::Dict policy =
      base::Value::Dict()
          .Set("string-policy", "value")
          .Set("string-enum-policy", "value-1")
          .Set("another-string-policy", 123)  // Test invalid policy value.
          .Set("int-policy", -123)
          .Set("int-enum-policy", 1)
          .Set("double-policy", 456e7)
          .Set("boolean-policy", true)
          .Set("list-policy",
               base::Value::List().Append("one").Append("two").Append("three"))
          .Set("dict-policy",
               base::Value::Dict().Set(
                   "list",
                   base::Value::List()
                       .Append(base::Value::Dict().Set("one", 1).Set("two", 2))
                       .Append(base::Value::Dict().Set("three", 3))));
  SetPolicies(policy);
  // Now run the extension.
  ASSERT_TRUE(RunExtensionTest("settings/managed_storage")) << message_;
}

// TODO(crbug.com/40789870): This test should be rewritten. See the bug for more
// details.
IN_PROC_BROWSER_TEST_P(ExtensionSettingsManagedStorageApiTest,
                       PRE_ManagedStorageEvents) {
  // This test starts without any test extensions installed.
  EXPECT_FALSE(GetSingleLoadedExtension());
  message_.clear();

  // Set policies for the test extension.
  base::Value::Dict policy = base::Value::Dict()
                                 .Set("constant-policy", "aaa")
                                 .Set("changes-policy", "bbb")
                                 .Set("deleted-policy", "ccc");
  SetPolicies(policy);

  ExtensionTestMessageListener ready_listener("ready");
  // Load the extension to install the event listener and wait for the
  // extension's registration to be stored since it must persist after
  // this PRE_ step exits. Otherwise, the test will be flaky, since the
  // extension's service worker registration might not get stored.
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("settings/managed_storage_events"),
      {.wait_for_registration_stored = true});
  ASSERT_TRUE(extension);
  // Wait until the extension sends the "ready" message.
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());

  // Now change the policies and wait until the extension is done.
  policy = base::Value::Dict()
               .Set("constant-policy", "aaa")
               .Set("changes-policy", "ddd")
               .Set("new-policy", "eee");
  SetPolicies(policy);
  EXPECT_TRUE(events_result_catcher_.GetNextResult())
      << events_result_catcher_.message();
}

IN_PROC_BROWSER_TEST_P(ExtensionSettingsManagedStorageApiTest,
                       ManagedStorageEvents) {
  // This test runs after PRE_ManagedStorageEvents without having deleted the
  // profile, so the extension is still around. While the browser restarted the
  // policy went back to the empty default, and so the extension should receive
  // the corresponding change events.

  // Verify that the test extension is still installed.
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension);
  EXPECT_EQ(kManagedStorageExtensionId, extension->id());

  // Running the test again skips the onInstalled callback, and just triggers
  // the onChanged notification.
  EXPECT_TRUE(events_result_catcher_.GetNextResult())
      << events_result_catcher_.message();
}

IN_PROC_BROWSER_TEST_P(ExtensionSettingsManagedStorageApiTest,
                       ManagedStorageDisabled) {
  // Disable the 'managed' namespace.
  StorageFrontend* frontend = StorageFrontend::Get(browser()->profile());
  frontend->DisableStorageForTesting(settings_namespace::MANAGED);
  EXPECT_FALSE(frontend->IsStorageEnabled(settings_namespace::MANAGED));
  // Now run the extension.
  ASSERT_TRUE(RunExtensionTest("settings/managed_storage_disabled"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionSettingsApiTest, StorageAreaOnChanged) {
  ASSERT_TRUE(RunExtensionTest("settings/storage_area")) << message_;
}

}  // namespace extensions
