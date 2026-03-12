// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/memory/ref_counted.h"
#include "base/one_shot_event.h"
#include "base/run_loop.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/storage/settings_sync_util.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/policy/schema_registry_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/crx_file/id_util.h"
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
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

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

  void SetPolicies(const base::DictValue& policies) {
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
    base::DictValue message;
    message.Set("namespace", StorageAreaToString(storage_area));
    message.Set("action", action);
    message.Set("isFinalAction", is_final_action);
    return base::WriteJson(message).value_or("");
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
  catcher.RestrictToBrowserContext(profile());
  catcher_incognito.RestrictToBrowserContext(
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true));

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
  catcher.RestrictToBrowserContext(profile());
  catcher_incognito.RestrictToBrowserContext(
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true));

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
  catcher.RestrictToBrowserContext(profile());
  catcher_incognito.RestrictToBrowserContext(
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true));

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
  catcher.RestrictToBrowserContext(profile());
  catcher_incognito.RestrictToBrowserContext(
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true));

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
  catcher.RestrictToBrowserContext(profile());
  catcher_incognito.RestrictToBrowserContext(
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true));

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
  StorageFrontend* frontend = StorageFrontend::Get(profile());
  EXPECT_TRUE(frontend->IsStorageEnabled(settings_namespace::LOCAL));
  EXPECT_TRUE(frontend->IsStorageEnabled(settings_namespace::SYNC));

  EXPECT_TRUE(frontend->IsStorageEnabled(settings_namespace::MANAGED));
}

using ContextType = extensions::browser_test_util::ContextType;

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

// Desktop Android supports only service worker, not persistent background.
#if BUILDFLAG(ENABLE_EXTENSIONS)
INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         ExtensionSettingsManagedStorageApiTest,
                         ::testing::Values(ContextType::kPersistentBackground));
#endif

INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         ExtensionSettingsManagedStorageApiTest,
                         ::testing::Values(ContextType::kServiceWorker));

IN_PROC_BROWSER_TEST_P(ExtensionSettingsManagedStorageApiTest,
                       ExtensionsSchemas) {
  // Verifies that the Schemas for the extensions domain are created on startup.
  ExtensionSystem* extension_system = ExtensionSystem::Get(profile());
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
      profile()->GetPolicySchemaRegistryService()->registry();
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
  base::DictValue policy =
      base::DictValue()
          .Set("string-policy", "value")
          .Set("string-enum-policy", "value-1")
          .Set("another-string-policy", 123)  // Test invalid policy value.
          .Set("int-policy", -123)
          .Set("int-enum-policy", 1)
          .Set("double-policy", 456e7)
          .Set("boolean-policy", true)
          .Set("list-policy",
               base::ListValue().Append("one").Append("two").Append("three"))
          .Set("dict-policy",
               base::DictValue().Set(
                   "list",
                   base::ListValue()
                       .Append(base::DictValue().Set("one", 1).Set("two", 2))
                       .Append(base::DictValue().Set("three", 3))));
  SetPolicies(policy);
  // Now run the extension.
  ASSERT_TRUE(RunExtensionTest("settings/managed_storage")) << message_;
}

// TODO(crbug.com/40200835): PRE_ test does not work on android_browsertest,
// therefore, disabled the following tests until the PRE_ test issue is
// resolved.
#if BUILDFLAG(ENABLE_EXTENSIONS)
// TODO(crbug.com/40789870): This test should be rewritten. See the bug for more
// details.
IN_PROC_BROWSER_TEST_P(ExtensionSettingsManagedStorageApiTest,
                       PRE_ManagedStorageEvents) {
  // This test starts without any test extensions installed.
  EXPECT_FALSE(GetSingleLoadedExtension());
  message_.clear();

  // Set policies for the test extension.
  base::DictValue policy = base::DictValue()
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
  policy = base::DictValue()
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
  // TODO(crbug.com/40200835): On desktop Android this assert fails because the
  // extension was not loaded. It's not clear why.
  ASSERT_TRUE(extension);
  EXPECT_EQ(kManagedStorageExtensionId, extension->id());

  // Running the test again skips the onInstalled callback, and just triggers
  // the onChanged notification.
  EXPECT_TRUE(events_result_catcher_.GetNextResult())
      << events_result_catcher_.message();
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

IN_PROC_BROWSER_TEST_P(ExtensionSettingsManagedStorageApiTest,
                       ManagedStorageDisabled) {
  // Disable the 'managed' namespace.
  StorageFrontend* frontend = StorageFrontend::Get(profile());
  frontend->DisableStorageForTesting(settings_namespace::MANAGED);
  EXPECT_FALSE(frontend->IsStorageEnabled(settings_namespace::MANAGED));
  // Now run the extension.
  ASSERT_TRUE(RunExtensionTest("settings/managed_storage_disabled"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionSettingsApiTest, StorageAreaOnChanged) {
  ASSERT_TRUE(RunExtensionTest("settings/storage_area")) << message_;
}

// The following matrix summarizes how chrome.storage.local (LevelDB) handles
// different corruption scenarios across the API:
//
// | Test Case                   | Corruption Type | Trigger | Recovery        |
// |-----------------------------|-----------------|---------|-----------------|
// | ReadInvalidJSON             | Logical (JSON)  | get()   | Key deleted     |
// | ReadInvalidJSONAfterRestart | Logical (JSON)  | Restart | Key deleted     |
// | RepairUnopenableDatabase... | Structural      | get()   | Database wiped  |
// | InvalidJsonClear            | Logical (JSON)  | clear() | Key deleted     |
// | ClearOnRead                 | Physical        | clear() | Database locked |
// | PhysicalBlockRot            | Physical        | get()   | Database locked |
//
using ExtensionCorruptLocalSettingsApiTest = ExtensionSettingsApiTest;

// Tests that logical (JSON) value corruption is handled gracefully on-read.
//
// Flow:
// 1. Manually insert invalid JSON strings into LevelDB for `test_key`.
// 2. Extension attempts `chrome.storage.local.get(['test_key'])`.
//
// Expectation:
// - The first read fails with an "Invalid JSON" error.
// - A subsequent read succeeds because `SettingsStorageQuotaEnforcer`
//   automatically deletes the corrupted key upon hitting the failure inline.
IN_PROC_BROWSER_TEST_F(ExtensionCorruptLocalSettingsApiTest, ReadInvalidJSON) {
  ExtensionTestMessageListener extension_background_loaded_listener(
      "background_ready", ReplyBehavior::kWillReply);
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("settings/storage_local/corruption"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(extension_background_loaded_listener.WaitUntilSatisfied());

  base::FilePath settings_dir =
      profile()->GetPath().AppendASCII("Local Extension Settings");
  base::FilePath db_path = settings_dir.AppendASCII(extension->id());

  // Corrupt a key in the extension's local settings.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    leveldb_env::Options options;
    options.create_if_missing = true;
    std::unique_ptr<leveldb::DB> db;
    leveldb::Status status =
        leveldb_env::OpenDB(options, db_path.AsUTF8Unsafe(), &db);
    ASSERT_TRUE(status.ok()) << status.ToString();

    // Write invalid JSON to simulate a corrupted value block.
    db->Put(leveldb::WriteOptions(), "test_key", "CORRUPT_JSON");
    // Write valid JSON for another key to show that even if part of the data is
    // valid the entire request will still fail.
    db->Put(leveldb::WriteOptions(), "good_key", "\"good_value\"");
    db.reset();
  }

  ExtensionTestMessageListener result_listener("get_error: Invalid JSON",
                                               ReplyBehavior::kWillReply);
  // Instruct extension background to attempt to read both database keys.
  extension_background_loaded_listener.Reply("get_keys");
  ASSERT_TRUE(result_listener.WaitUntilSatisfied());

  // Attempt a second read to see if the database repair (which deleted the
  // key) allows subsequent reads to succeed.
  ExtensionTestMessageListener after_repair_result_listener(
      "get_success: {\"good_key\":\"good_value\"}");
  result_listener.Reply("get_keys");
  ASSERT_TRUE(after_repair_result_listener.WaitUntilSatisfied());
}

// TODO(crbug.com/480952785): PRE_ tests with local state seem flaky on android.
#if !BUILDFLAG(IS_ANDROID)

// Tests that setting data via chrome.storage.local, restarting the browser, and
// then discovering data corruption upon the next read after restart results in
// an error for the read request. The PRE_ test sets up the initial DB
// state and then corrupts a key. The browser restart after corruption isn't
// enough to repair the database, a storage operation must be made against it
// first.
IN_PROC_BROWSER_TEST_F(ExtensionCorruptLocalSettingsApiTest,
                       PRE_ReadInvalidJSONAfterRestart) {
  ExtensionTestMessageListener extension_background_loaded_listener(
      "background_ready", ReplyBehavior::kWillReply);

  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("settings/storage_local/corruption"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(extension_background_loaded_listener.WaitUntilSatisfied());

  base::FilePath settings_dir =
      profile()->GetPath().AppendASCII("Local Extension Settings");
  base::FilePath db_path = settings_dir.AppendASCII(extension->id());

  // Construct and corrupt a key in the extension's local settings.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    leveldb_env::Options options;
    options.create_if_missing = true;
    std::unique_ptr<leveldb::DB> db;
    leveldb::Status status =
        leveldb_env::OpenDB(options, db_path.AsUTF8Unsafe(), &db);
    ASSERT_TRUE(status.ok()) << status.ToString();

    // Overwrite the previously valid JSON with invalid JSON.
    db->Put(leveldb::WriteOptions(), "test_key", "CORRUPT_JSON");
    // Write valid JSON for another key.
    db->Put(leveldb::WriteOptions(), "good_key", "\"good_value\"");
    db.reset();  // Release lock.
  }
  extension_background_loaded_listener.Reply(
      "");  // Release JS execution thread
}

// Tests that simply opening a database with invalid JSON values after browser
// restart does not repair it.
//
// Flow (PRE_ step previously wrote invalid JSON for `test_key`):
// 1. Load the extension (triggering `LazyLevelDb` opening the valid MANIFEST).
// 2. Extension reads keys.
//
// Expectation:
// - The first read fails with an "Invalid JSON" error, proving opening didn't
// repair.
// - A subsequent read succeeds because the fail on-read triggered deletion of
// the key.
IN_PROC_BROWSER_TEST_F(ExtensionCorruptLocalSettingsApiTest,
                       ReadInvalidJSONAfterRestart) {
  // Load the extension that we previously populated in the PRE_ test.
  // Because the database's MANIFEST is still valid,
  // `LazyLevelDb::EnsureDbIsOpen()` will succeed upon opening. This proves that
  // simply opening the DB does not fix the corruption if the issue is deep
  // within the value data blocks.
  ExtensionTestMessageListener extension_background_loaded_listener(
      "background_ready", ReplyBehavior::kWillReply);
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("settings/storage_local/corruption"));
  ASSERT_TRUE(extension);

  ASSERT_TRUE(extension_background_loaded_listener.WaitUntilSatisfied());

  // Instruct extension background to attempt to read both database keys, one of
  // which is corrupt. The fact that we get an error means that the browser
  // restart by itself (after PRE_) doesn't repair the database. Since we're
  // working with a `LazyLevelDb`, only the first operation against the database
  // after browser start will attempt to repair.
  ExtensionTestMessageListener result_listener("get_error: Invalid JSON",
                                               ReplyBehavior::kWillReply);
  extension_background_loaded_listener.Reply("get_keys");
  ASSERT_TRUE(result_listener.WaitUntilSatisfied());

  // Attempt a second read to see if the database repair (which deleted the
  // key) allows subsequent reads to succeed.
  ExtensionTestMessageListener after_repair_result_listener(
      "get_success: {\"good_key\":\"good_value\"}");
  after_repair_result_listener.set_failure_message("get_error: Invalid JSON");
  result_listener.Reply("get_keys");
  ASSERT_TRUE(after_repair_result_listener.WaitUntilSatisfied());
}

// Tests the behavior when the LevelDB MANIFEST file itself is completely
// corrupted or missing. The PRE_ test sets up the initial valid state.
IN_PROC_BROWSER_TEST_F(ExtensionCorruptLocalSettingsApiTest,
                       PRE_RepairUnopenableDatabaseOnBrowserStart) {
  // Load the extension and instruct it to write valid data to the
  // database. This ensures the LevelDB database files, MANIFEST, and logs
  // are fully generated on disk before the browser restarts for the main test.
  ExtensionTestMessageListener extension_background_loaded_listener(
      "background_ready", ReplyBehavior::kWillReply);
  ExtensionTestMessageListener set_success_listener("set_success");

  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("settings/storage_local/corruption"));
  ASSERT_TRUE(extension);

  ASSERT_TRUE(extension_background_loaded_listener.WaitUntilSatisfied());
  extension_background_loaded_listener.Reply("set_data");
  ASSERT_TRUE(set_success_listener.WaitUntilSatisfied());
}

// Tests that severe database structural corruption (unopenable MANIFEST)
// triggers a full destructive database wipe on access.
//
// Flow (PRE_ step wrote valid data, and we then corrupted all files with
// garbage):
// 1. Load extension; LevelDB open step hits severe corruption.
// 2. Extension attempts a standard get everything (`get(null)`) read.
//
// Expectation:
// - `LazyLevelDb` executes `FixCorruption()` destroying all locked files.
// - The read eventually succeeds but returns empty `{}`, because the repair
// wiped it.
IN_PROC_BROWSER_TEST_F(ExtensionCorruptLocalSettingsApiTest,
                       RepairUnopenableDatabaseOnBrowserStart) {
  ExtensionTestMessageListener extension_background_loaded_listener(
      "background_ready", ReplyBehavior::kWillReply);
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("settings/storage_local/corruption"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(extension_background_loaded_listener.WaitUntilSatisfied());

  base::FilePath settings_dir =
      profile()->GetPath().AppendASCII("Local Extension Settings");
  base::FilePath db_path = settings_dir.AppendASCII(extension->id());

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    // Traverse the DB directory and overwrite every file with garbage. This
    // corrupts the MANIFEST, CURRENT, and .ldb files guaranteeing a total
    // failure when the database is opened to service the read operation.
    // This is safe to do because `LazyLevelDb` doesn't open the database until
    // the first API operation.
    const std::string kGarbage("I am a corrupted leveldb file.");
    base::FileEnumerator enumerator(db_path, true /* recursive */,
                                    base::FileEnumerator::FILES);
    for (base::FilePath file = enumerator.Next(); !file.empty();
         file = enumerator.Next()) {
      ASSERT_TRUE(base::WriteFile(file, kGarbage));
    }
  }
  ASSERT_TRUE(extension_background_loaded_listener.WaitUntilSatisfied());

  // Tell the extension to attempt a chrome.storage.local.get(). This triggers
  // `LazyLevelDb` to open the database. Because the MANIFEST is corrupt, OpenDB
  // will fail with CORRUPTION. This triggers an automatic destructive repair
  // (`LazyLevelDb::FixCorruption()`) which will permanently delete the entire
  // DB in this case.
  ExtensionTestMessageListener result_listener;
  extension_background_loaded_listener.Reply("get_all");
  ASSERT_TRUE(result_listener.WaitUntilSatisfied());
  std::string response = result_listener.message();

  // Verify the read operation still succeeds but returns empty data because the
  // DB was wiped clean during the repair.
  EXPECT_EQ(response, "get_all_success: {}");
}

#endif  // !BUILDFLAG(IS_ANDROID)

// Tests that heavy sequential operations like `clear()` fail immediately when
// faced with severely corrupted physical table wreckage.
//
// Flow (Manual raw LevelDB creates valid structure, then destroys with .ldb
// garbage):
// 1. Extension attempts `chrome.storage.local.clear()`.
//
// Expectation:
// - `clear()` triggers index traversals that encounter the wrecked table data.
// - Operation fails with lockout instead of wiping destructively.
// - Any subsequent `clear()` or `get()` calls also fail because LevelDB doesn't
// attempt any repair in this case.
IN_PROC_BROWSER_TEST_F(ExtensionCorruptLocalSettingsApiTest, ClearOnRead) {
  ExtensionTestMessageListener extension_background_loaded_listener(
      "background_ready", ReplyBehavior::kWillReply);
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("settings/storage_local/corruption"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(extension_background_loaded_listener.WaitUntilSatisfied());

  base::FilePath settings_dir =
      profile()->GetPath().AppendASCII("Local Extension Settings");
  base::FilePath db_path = settings_dir.AppendASCII(extension->id());

  // Use raw LevelDB to construct a valid database.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    leveldb_env::Options options;
    options.create_if_missing = true;
    std::unique_ptr<leveldb::DB> db;
    leveldb::Status status =
        leveldb_env::OpenDB(options, db_path.AsUTF8Unsafe(), &db);
    ASSERT_TRUE(status.ok()) << status.ToString();

    db->Put(leveldb::WriteOptions(), "test_key", "\"test_value\"");
    db->Put(leveldb::WriteOptions(), "good_key", "\"good_value\"");

    // Force a compaction. This guarantees the data is written to a physical
    // .ldb file on disk instead of lingering in the .log memtable.
    db->CompactRange(nullptr, nullptr);
    db.reset();  // Release the database lock so the extension can access it.
  }

  // Simulate natural block corruption by injecting garbage directly into the
  // newly generated .ldb file. This will cause a physical block checksum
  // mismatch.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    const std::string kGarbage("I am a corrupted leveldb file.");
    base::FileEnumerator enumerator(db_path, /*recursive=*/true,
                                    base::FileEnumerator::FILES);
    for (base::FilePath file = enumerator.Next(); !file.empty();
         file = enumerator.Next()) {
      if (file.Extension() == FILE_PATH_LITERAL(".ldb")) {
        // Overwrite the actual inode so the open file descriptor reads garbage.
        ASSERT_TRUE(base::WriteFile(file, kGarbage));
      }
    }
  }

  // Instruct the extension to attempt to clear the database.
  ExtensionTestMessageListener clear_result_listener(
      "clear_error: Corruption: not an sstable (footer too short)",
      ReplyBehavior::kWillReply);
  extension_background_loaded_listener.Reply("clear");
  // Verify that calling chrome.storage.local.clear() creates an iterator which
  // hits the corrupted .ldb block and fails immediately.
  EXPECT_TRUE(clear_result_listener.WaitUntilSatisfied());

  // Verify that a second attempt to clear the database also fails, proving that
  // the first failed clear didn't repair or delete the database.
  ExtensionTestMessageListener second_clear_listener(
      "clear_error: Corruption: not an sstable (footer too short)",
      ReplyBehavior::kWillReply);
  clear_result_listener.Reply("clear");
  EXPECT_TRUE(second_clear_listener.WaitUntilSatisfied());

  // Verify that a get() for the corrupted key fails.
  ExtensionTestMessageListener get_listener(
      "get_test_key_error: Corruption: not an sstable (footer too short)",
      ReplyBehavior::kWillReply);
  second_clear_listener.Reply("get_test_key");
  EXPECT_TRUE(get_listener.WaitUntilSatisfied());

  // Verify that getting a different, non-corrupted key also fails because the
  // entire database is locked out.
  ExtensionTestMessageListener get_good_key_listener(
      "get_good_key_error: Corruption: not an sstable (footer too short)");
  get_listener.Reply("get_good_key");
  EXPECT_TRUE(get_good_key_listener.WaitUntilSatisfied());
}

// Tests that `clear()` handles logical JSON corruption gracefully without
// lockout.
//
// Flow:
// 1. Write invalid JSON to raw LevelDB but maintain structural health.
// 2. Extension attempts `chrome.storage.local.clear()`.
//
// Expectation:
// - The quota enforcer reads the invalid JSON and deletes the key inline.
// - Subsequence .clear() succeeds resolving normally.
// - `onChanged` triggers fires with only the uncorrupted key data (not the
// corrupted key that was deleted inline earlier).
IN_PROC_BROWSER_TEST_F(ExtensionCorruptLocalSettingsApiTest, InvalidJsonClear) {
  ExtensionTestMessageListener extension_background_loaded_listener(
      "background_ready", ReplyBehavior::kWillReply);
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("settings/storage_local/corruption"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(extension_background_loaded_listener.WaitUntilSatisfied());

  base::FilePath settings_dir =
      profile()->GetPath().AppendASCII("Local Extension Settings");
  base::FilePath db_path = settings_dir.AppendASCII(extension->id());

  // Use raw LevelDB to construct a database with valid blocks
  // but invalid JSON payloads.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    leveldb_env::Options options;
    options.create_if_missing = true;
    std::unique_ptr<leveldb::DB> db;
    leveldb::Status status =
        leveldb_env::OpenDB(options, db_path.AsUTF8Unsafe(), &db);
    ASSERT_TRUE(status.ok()) << status.ToString();

    // Write invalid JSON to simulate a logical corruption.
    // Unlike a block read error, the leveldb structures are perfectly fine.
    db->Put(leveldb::WriteOptions(), "test_key", "CORRUPT_JSON");
    // Write valid JSON for the good key.
    db->Put(leveldb::WriteOptions(), "good_key", "\"good_value\"");
    db.reset();  // close the DB
  }

  // Instruct the extension to attempt to clear the database and verify that
  // the `chrome.storage.onChanged` event (for local namespace) is fired for the
  // uncorrupted key.
  ExtensionTestMessageListener clear_listener("clear_success");
  // `test_key` (corrupted) is deleted inline during a quota read operation
  // before `clear()` executes, so `onChanged()` only reports updates for
  // `good_key`.
  ExtensionTestMessageListener on_changed_listener(
      "on_changed_success: {\"good_key\":{\"oldValue\":\"good_value\"}}");

  extension_background_loaded_listener.Reply("clear");

  // Verify that calling chrome.storage.local.clear() triggers both responses
  // correctly without order flakiness.
  EXPECT_TRUE(clear_listener.WaitUntilSatisfied());
  EXPECT_TRUE(on_changed_listener.WaitUntilSatisfied());
}

// Tests that localized physical bit rot (checksum mismatch) locks out the
// entire database session from access or clears.
//
// Flow:
// 1. Compress valid data to disk and inject single "GARBAGE" byte into .ldb
// payload.
// 2. Extension reads corrupted key -> fails with block checksum mismatch.
// 3. Reload the extension to flush LevelDB memory/block caches. Otherwise,
//    subsequent reads might resolve from memory, making the database appear
//    undamaged.
// 4. Extension attempts full database read (`get(null)`) and fails.
//
// Expectation:
// - Incremental `clear()` or `get(good_key)` operations continue to fail after
// encountering block checksum mismatch.
// - Physical block failures constitute non-recoverable session fatalities.
IN_PROC_BROWSER_TEST_F(ExtensionCorruptLocalSettingsApiTest, PhysicalBlockRot) {
  ExtensionTestMessageListener extension_background_loaded_listener(
      "background_ready", ReplyBehavior::kWillReply);
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("settings/storage_local/corruption"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(extension_background_loaded_listener.WaitUntilSatisfied());

  base::FilePath settings_dir =
      profile()->GetPath().AppendASCII("Local Extension Settings");
  base::FilePath db_path = settings_dir.AppendASCII(extension->id());

  // Use raw LevelDB to construct a valid database with a huge payload.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    leveldb_env::Options options;
    options.create_if_missing = true;
    std::unique_ptr<leveldb::DB> db;
    leveldb::Status status =
        leveldb_env::OpenDB(options, db_path.AsUTF8Unsafe(), &db);
    ASSERT_TRUE(status.ok()) << status.ToString();

    // Write a regular value and use CompactRange to guarantee it's flushed
    // from the MemTable to an .ldb file on disk.
    std::string huge_payload =
        "\"HUGE_PAYLOAD_" + std::string(10000, 'a') + "\"";
    db->Put(leveldb::WriteOptions(), "test_key", huge_payload);
    db->Put(leveldb::WriteOptions(), "good_key", "\"good_value\"");

    // Force a compaction to flush the MemTable to an .ldb file.
    db->CompactRange(nullptr, nullptr);
    db.reset();  // Close the DB safely.
  }

  // Simulate disk rot by injecting garbage data right into the middle of
  // the .ldb file's payload, invalidating the checksum.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    bool found_ldb = false;
    base::FilePath ldb_path;

    base::FileEnumerator enumerator(db_path, /*recursive=*/true,
                                    base::FileEnumerator::FILES);
    for (base::FilePath file = enumerator.Next(); !file.empty();
         file = enumerator.Next()) {
      if (file.Extension() == FILE_PATH_LITERAL(".ldb")) {
        found_ldb = true;
        ldb_path = file;
      }
    }
    ASSERT_TRUE(found_ldb) << "Could not find the compacted .ldb file.";

    int64_t original_size = base::GetFileSize(ldb_path).value_or(0);
    ASSERT_GT(original_size, 100);

    base::File file(ldb_path, base::File::FLAG_OPEN | base::File::FLAG_WRITE);
    ASSERT_TRUE(file.IsValid());

    // Seek to exactly the middle of the file (in the middle of a data
    // block) and write some garbage to invalidate the checksum.
    file.Seek(base::File::FROM_BEGIN, original_size / 2);
    const char kRot[] = "GARBAGE";
    ASSERT_TRUE(file.WriteAtCurrentPosAndCheck(base::as_byte_span(kRot)));
  }

  // Instruct the extension to have it attempt a targeted get() of the corrupted
  // key. When we specifically ask for a key, `LazyLevelDb::Read()` hits the
  // rotted block and returns a generic block checksum mismatch error.
  ExtensionTestMessageListener get_when_key_corrupted_listener(
      ReplyBehavior::kWillReply);
  extension_background_loaded_listener.Reply("get_test_key");
  ASSERT_TRUE(get_when_key_corrupted_listener.WaitUntilSatisfied());
  std::string get_response = get_when_key_corrupted_listener.message();

  EXPECT_TRUE(base::StartsWith(get_response, "get_test_key_error:"));
  EXPECT_NE(get_response.find("block checksum mismatch"), std::string::npos);
  get_when_key_corrupted_listener.Reply("");  // Release JS execution thread

  // Reload the extension to clear LevelDB memory/block caches and avoid reads
  // resolving from memory buffers. This forces the database to be read
  // directly from the files on disk that contain the physical rot.
  ExtensionTestMessageListener post_reload_ready_listener(
      "background_ready", ReplyBehavior::kWillReply);
  ReloadExtension(extension->id());
  ASSERT_TRUE(post_reload_ready_listener.WaitUntilSatisfied());

  // Instruct the extension to attempt a full database read with
  // `chrome.storage.local.get(null)`. When we ask for all data, the LevelDB
  // iterator crashes when it crosses into the rotted block. Because it crashes
  // before reading the data, it does not know the key name.
  ExtensionTestMessageListener full_database_get_listener(
      ReplyBehavior::kWillReply);
  post_reload_ready_listener.Reply("get_all");

  ASSERT_TRUE(full_database_get_listener.WaitUntilSatisfied());
  std::string full_database_get_response = full_database_get_listener.message();

  // The database is still corrupted. The scan hits the rotted block and fails.
  // Notice the error string does NOT contain any key name (like "_key'").
  EXPECT_TRUE(base::StartsWith(full_database_get_response, "get_all_error:"));
  EXPECT_NE(full_database_get_response.find("block checksum mismatch"),
            std::string::npos);
  EXPECT_EQ(full_database_get_response.find("_key'"), std::string::npos);

  // Verify clear() fails with checksum mismatch.
  ExtensionTestMessageListener clear_listener(ReplyBehavior::kWillReply);
  full_database_get_listener.Reply("clear");
  ASSERT_TRUE(clear_listener.WaitUntilSatisfied());
  std::string clear_response = clear_listener.message();
  EXPECT_TRUE(base::StartsWith(clear_response, "clear_error:"));
  EXPECT_NE(clear_response.find("block checksum mismatch"), std::string::npos);

  // Verify getting corrupted Key still fails with checksum mismatch.
  ExtensionTestMessageListener get_test_listener(ReplyBehavior::kWillReply);
  clear_listener.Reply("get_test_key");
  ASSERT_TRUE(get_test_listener.WaitUntilSatisfied());
  std::string get_test_response = get_test_listener.message();
  EXPECT_TRUE(base::StartsWith(get_test_response, "get_test_key_error:"));
  EXPECT_NE(get_test_response.find("block checksum mismatch"),
            std::string::npos);

  // Verify getting the uncorrupted key also fails.
  ExtensionTestMessageListener get_good_listener(ReplyBehavior::kWillReply);
  get_test_listener.Reply("get_good_key");
  ASSERT_TRUE(get_good_listener.WaitUntilSatisfied());
  std::string get_good_response = get_good_listener.message();
  EXPECT_TRUE(base::StartsWith(get_good_response, "get_good_key_error:"));
  EXPECT_NE(get_good_response.find("block checksum mismatch"),
            std::string::npos);
  get_good_listener.Reply("");  // Release JS execution thread
}

}  // namespace extensions
