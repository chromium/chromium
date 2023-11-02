// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/system_extensions/system_extensions_install_manager.h"
#include "chrome/browser/ash/system_extensions/system_extensions_persistent_storage.h"
#include "chrome/browser/ash/system_extensions/system_extensions_profile_utils.h"
#include "chrome/browser/ash/system_extensions/system_extensions_provider.h"
#include "chrome/browser/ash/system_extensions/system_extensions_provider_factory.h"
#include "chrome/browser/ash/system_extensions/system_extensions_service_worker_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/common/page_type.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"

namespace ash {

namespace {

constexpr SystemExtensionId kTestSystemExtensionId = {1, 2, 3, 4};

constexpr char kTestSystemExtensionManifest[] = R"({
   "id": "01020304",
   "name": "Sample System Web Extension",
   "service_worker_url": "/sw.js",
   "short_name": "Sample SWX",
   "type": "window-management"
}
)";

constexpr char kTestSystemExtensionIndexURL[] =
    "chrome-untrusted://system-extension-window-management-01020304/html/"
    "index.html";

constexpr char kTestSystemExtensionWrongURL[] =
    "chrome-untrusted://system-extension-window-management-01020304/html/"
    "wrong.html";

constexpr char kTestSystemExtensionEmptyPathURL[] =
    "chrome-untrusted://system-extension-window-management-01020304/";

base::FilePath GetBasicSystemExtensionDir() {
  base::FilePath test_dir;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_dir);
  return test_dir.Append("system_extensions").Append("basic_system_extension");
}

base::FilePath GetManagedDeviceHealthServicesExtensionDir() {
  base::FilePath test_dir;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_dir);
  return test_dir.Append("system_extensions")
      .Append("managed_device_health_services_extension");
}

// Wrapper around base::OneShotEvent that allows callers to signal with
// arguments.
template <typename... Args>
class OneShotEventWrapper {
 public:
  OneShotEventWrapper() = default;

  ~OneShotEventWrapper() = default;

  void Signal(Args... args) {
    run_with_args_ = base::BindRepeating(&OneShotEventWrapper::RunWithArgs,
                                         weak_ptr_factory_.GetWeakPtr(),
                                         std::forward<Args>(args)...);
    one_shot_event_.Signal();
  }

  void Post(const base::Location& from_here,
            base::OnceCallback<void(Args...)> task) {
    one_shot_event_.Post(
        from_here,
        base::BindOnce(&OneShotEventWrapper::RunTask,
                       weak_ptr_factory_.GetWeakPtr(), std::move(task)));
  }

 private:
  void RunTask(base::OnceCallback<void(Args...)> task) {
    run_with_args_.Run(std::move(task));
  }

  void RunWithArgs(Args... args, base::OnceCallback<void(Args...)> task) {
    std::move(task).Run(std::forward<Args>(args)...);
  }

  base::OneShotEvent one_shot_event_;
  base::RepeatingCallback<void(base::OnceCallback<void(Args...)>)>
      run_with_args_;
  base::WeakPtrFactory<OneShotEventWrapper> weak_ptr_factory_{this};
};

// Class that can be used to wait for events triggered during installation. If
// an event is triggered more than once, this class will CHECK.
class TestInstallationEventsWaiter
    : public SystemExtensionsInstallManager::Observer,
      public SystemExtensionsServiceWorkerManager::Observer {
 public:
  explicit TestInstallationEventsWaiter(SystemExtensionsProvider& provider) {
    service_worker_manager_observation_.Observe(
        &provider.service_worker_manager());
    install_manager_observation_.Observe(&provider.install_manager());
  }

  ~TestInstallationEventsWaiter() override = default;

  // Returns the result of a Service Worker registration. Waits if there hasn't
  // been one yet.
  std::pair<SystemExtensionId, blink::ServiceWorkerStatusCode>
  WaitForServiceWorkerRegistered() {
    absl::optional<SystemExtensionId> id;
    absl::optional<blink::ServiceWorkerStatusCode> status_code;

    base::RunLoop run_loop;
    on_register_service_worker_.Post(
        FROM_HERE,
        base::BindLambdaForTesting(
            [&](SystemExtensionId returned_id,
                blink::ServiceWorkerStatusCode returned_status_code) {
              id = returned_id;
              status_code = returned_status_code;
              run_loop.Quit();
            }));
    run_loop.Run();

    return {id.value(), status_code.value()};
  }

  // Returns the result of a Service Worker unregistration. Waits if there
  // hasn't been one yet.
  std::pair<SystemExtensionId, bool> WaitForServiceWorkerUnregistered() {
    absl::optional<SystemExtensionId> id;
    absl::optional<bool> succeeded;

    base::RunLoop run_loop;
    on_unregister_service_worker_.Post(
        FROM_HERE, base::BindLambdaForTesting([&](SystemExtensionId returned_id,
                                                  bool returned_succeeded) {
          id = returned_id;
          succeeded = returned_succeeded;
          run_loop.Quit();
        }));
    run_loop.Run();

    return {id.value(), succeeded.value()};
  }

  // Returns the result of a asset deletion operations. Waits if there
  // hasn't been one yet.
  std::pair<SystemExtensionId, bool> WaitForAssetsDeleted() {
    absl::optional<SystemExtensionId> id;
    absl::optional<bool> succeeded;

    base::RunLoop run_loop;
    on_assets_deleted_.Post(
        FROM_HERE, base::BindLambdaForTesting([&](SystemExtensionId returned_id,
                                                  bool returned_succeeded) {
          id = returned_id;
          succeeded = returned_succeeded;
          run_loop.Quit();
        }));
    run_loop.Run();

    return {id.value(), succeeded.value()};
  }

  // SystemExtensionsServiceWorkerManager::Observer
  void OnRegisterServiceWorker(
      const SystemExtensionId& id,
      blink::ServiceWorkerStatusCode status_code) override {
    on_register_service_worker_.Signal(id, status_code);
  }

  void OnUnregisterServiceWorker(const SystemExtensionId& id,
                                 bool succeeded) override {
    on_unregister_service_worker_.Signal(id, succeeded);
  }

  // SystemExtensionsInstallManager::Observer
  void OnSystemExtensionAssetsDeleted(const SystemExtensionId& id,
                                      bool succeeded) override {
    on_assets_deleted_.Signal(id, succeeded);
  }

 private:
  OneShotEventWrapper<SystemExtensionId, blink::ServiceWorkerStatusCode>
      on_register_service_worker_;
  OneShotEventWrapper<SystemExtensionId, bool> on_unregister_service_worker_;
  OneShotEventWrapper<SystemExtensionId, bool> on_assets_deleted_;

  base::ScopedObservation<SystemExtensionsServiceWorkerManager,
                          SystemExtensionsServiceWorkerManager::Observer>
      service_worker_manager_observation_{this};

  base::ScopedObservation<SystemExtensionsInstallManager,
                          SystemExtensionsInstallManager::Observer>
      install_manager_observation_{this};
};

class SystemExtensionsBrowserTest : public InProcessBrowserTest {
 public:
  SystemExtensionsBrowserTest() {
    feature_list_.InitWithFeatures(
        {features::kSystemExtensions,
         ::features::kEnableServiceWorkersForChromeUntrusted},
        {});
  }
  ~SystemExtensionsBrowserTest() override = default;

  void TestInstalledTestExtensionWorks() {
    auto& provider = SystemExtensionsProvider::Get(browser()->profile());
    auto& registry = provider.registry();

    auto extension_ids = registry.GetIds();
    EXPECT_EQ(std::vector<SystemExtensionId>({kTestSystemExtensionId}),
              extension_ids);
    EXPECT_TRUE(registry.GetById(kTestSystemExtensionId));

    // Test we persisted the System Extension.
    absl::optional<SystemExtensionPersistedInfo> persistence_info =
        provider.persistent_storage().Get(kTestSystemExtensionId);
    ASSERT_TRUE(persistence_info);
    EXPECT_EQ(kTestSystemExtensionManifest,
              persistence_info->manifest.DebugString());

    auto* tab = browser()->tab_strip_model()->GetActiveWebContents();
    {
      ASSERT_TRUE(ui_test_utils::NavigateToURL(
          browser(), GURL(kTestSystemExtensionIndexURL)));
      EXPECT_EQ(u"SystemExtension", tab->GetTitle());
    }
    {
      // Check that navigating to non-existing resources doesn't crash the
      // browser.
      ASSERT_TRUE(ui_test_utils::NavigateToURL(
          browser(), GURL(kTestSystemExtensionWrongURL)));
      content::NavigationEntry* entry = tab->GetController().GetVisibleEntry();
      EXPECT_EQ(content::PAGE_TYPE_ERROR, entry->GetPageType());
    }
    {
      // Check that navigating to a directory, like the root directory, doesn't
      // crash the browser.
      ASSERT_TRUE(ui_test_utils::NavigateToURL(
          browser(), GURL(kTestSystemExtensionEmptyPathURL)));
      content::NavigationEntry* entry = tab->GetController().GetVisibleEntry();
      EXPECT_EQ(content::PAGE_TYPE_ERROR, entry->GetPageType());
    }
  }

  void TestExtensionUninstalled() {
    auto& provider = SystemExtensionsProvider::Get(browser()->profile());
    auto& registry = provider.registry();

    EXPECT_TRUE(registry.GetIds().empty());
    EXPECT_FALSE(registry.GetById(kTestSystemExtensionId));

    // Tests that the System Extension is no longer in persistent storage.
    absl::optional<SystemExtensionPersistedInfo> persistence_info =
        provider.persistent_storage().Get(kTestSystemExtensionId);
    EXPECT_FALSE(persistence_info);

    // Test that navigating to the System Extension's resources fails.
    auto* tab = browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), GURL(kTestSystemExtensionIndexURL)));
    content::NavigationEntry* entry = tab->GetController().GetVisibleEntry();
    EXPECT_EQ(content::PAGE_TYPE_ERROR, entry->GetPageType());

    {
      // Test that the resources have been deleted.
      base::ScopedAllowBlockingForTesting allow_blocking;
      const base::FilePath system_extension_dir =
          GetDirectoryForSystemExtension(*browser()->profile(),
                                         kTestSystemExtensionId);
      EXPECT_FALSE(base::PathExists(system_extension_dir));
    }

    {
      // Test that the service worker has been unregistered.
      const GURL scope(kTestSystemExtensionEmptyPathURL);
      auto* worker_context = browser()
                                 ->profile()
                                 ->GetDefaultStoragePartition()
                                 ->GetServiceWorkerContext();

      base::RunLoop run_loop;
      worker_context->CheckHasServiceWorker(
          scope, blink::StorageKey(url::Origin::Create(scope)),
          base::BindLambdaForTesting(
              [&](content::ServiceWorkerCapability capability) {
                EXPECT_EQ(capability,
                          content::ServiceWorkerCapability::NO_SERVICE_WORKER);
                run_loop.Quit();
              }));
      run_loop.Run();
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class SystemExtensionsSwitchBrowserTest : public SystemExtensionsBrowserTest {
 public:
  ~SystemExtensionsSwitchBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchPath(ash::switches::kInstallSystemExtension,
                                   GetBasicSystemExtensionDir());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class SystemExtensionsBrowserTestWithManagedDeviceHealthServicesPreTest
    : public SystemExtensionsBrowserTest {
 public:
  SystemExtensionsBrowserTestWithManagedDeviceHealthServicesPreTest() {
    // Only enable the feature flag if this is the pre-test.
    if (content::IsPreTest()) {
      feature_list_.InitAndEnableFeature(
          features::kSystemExtensionsManagedDeviceHealthServices);
    }
  }

  ~SystemExtensionsBrowserTestWithManagedDeviceHealthServicesPreTest()
      override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(SystemExtensionsBrowserTest, InstallFromDir_Success) {
  auto& provider = SystemExtensionsProvider::Get(browser()->profile());
  auto& install_manager = provider.install_manager();

  TestInstallationEventsWaiter waiter(provider);
  base::RunLoop run_loop;
  install_manager.InstallUnpackedExtensionFromDir(
      GetBasicSystemExtensionDir(),
      base::BindLambdaForTesting([&](InstallStatusOrSystemExtensionId result) {
        EXPECT_TRUE(result.ok());
        EXPECT_EQ(kTestSystemExtensionId, result.value());
        run_loop.Quit();
      }));
  run_loop.Run();

  const auto [id, status_code] = waiter.WaitForServiceWorkerRegistered();
  EXPECT_EQ(kTestSystemExtensionId, id);
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status_code);

  TestInstalledTestExtensionWorks();
}

IN_PROC_BROWSER_TEST_F(SystemExtensionsBrowserTest, Uninstall_Success) {
  auto& provider = SystemExtensionsProvider::Get(browser()->profile());
  auto& install_manager = provider.install_manager();

  TestInstallationEventsWaiter waiter(provider);

  {
    // Install and wait for the service worker to be registered.
    base::RunLoop run_loop;
    install_manager.InstallUnpackedExtensionFromDir(
        GetBasicSystemExtensionDir(),
        base::BindLambdaForTesting(
            [&](InstallStatusOrSystemExtensionId result) { run_loop.Quit(); }));
    run_loop.Run();
    waiter.WaitForServiceWorkerRegistered();
  }

  // Uninstall the extension.
  install_manager.Uninstall(kTestSystemExtensionId);

  {
    const auto [id, deletion_succeeded] = waiter.WaitForAssetsDeleted();
    EXPECT_EQ(id, kTestSystemExtensionId);
    EXPECT_TRUE(deletion_succeeded);
  }

  {
    const auto [id, unregistration_succeeded] =
        waiter.WaitForServiceWorkerUnregistered();
    EXPECT_EQ(id, kTestSystemExtensionId);
    EXPECT_TRUE(unregistration_succeeded);
  }

  TestExtensionUninstalled();
}

// Tests that extensions are persisted across restarts.
IN_PROC_BROWSER_TEST_F(SystemExtensionsBrowserTest,
                       PRE_PersistedAcrossRestart) {
  auto& provider = SystemExtensionsProvider::Get(browser()->profile());
  auto& install_manager = provider.install_manager();

  TestInstallationEventsWaiter waiter(provider);

  {
    // Install and wait for the service worker to be registered.
    base::RunLoop run_loop;
    install_manager.InstallUnpackedExtensionFromDir(
        GetBasicSystemExtensionDir(),
        base::BindLambdaForTesting(
            [&](InstallStatusOrSystemExtensionId result) { run_loop.Quit(); }));
    run_loop.Run();
    waiter.WaitForServiceWorkerRegistered();
  }
}

IN_PROC_BROWSER_TEST_F(SystemExtensionsBrowserTest, PersistedAcrossRestart) {
  auto& provider = SystemExtensionsProvider::Get(browser()->profile());
  auto& install_manager = provider.install_manager();

  // Wait for previously persisted System Extensions to be registered.
  base::RunLoop run_loop;
  install_manager.on_register_previously_persisted_finished().Post(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  TestInstalledTestExtensionWorks();
}

// Tests that an extension can be uninstalled after restart.
IN_PROC_BROWSER_TEST_F(SystemExtensionsBrowserTest, PRE_UninstallAfterRestart) {
  auto& provider = SystemExtensionsProvider::Get(browser()->profile());
  auto& install_manager = provider.install_manager();

  TestInstallationEventsWaiter waiter(provider);

  {
    // Install and wait for the service worker to be registered.
    base::RunLoop run_loop;
    install_manager.InstallUnpackedExtensionFromDir(
        GetBasicSystemExtensionDir(),
        base::BindLambdaForTesting(
            [&](InstallStatusOrSystemExtensionId result) { run_loop.Quit(); }));
    run_loop.Run();
    waiter.WaitForServiceWorkerRegistered();
  }
}

IN_PROC_BROWSER_TEST_F(SystemExtensionsBrowserTest, UninstallAfterRestart) {
  auto& provider = SystemExtensionsProvider::Get(browser()->profile());
  auto& install_manager = provider.install_manager();

  // Wait for previously persisted System Extensions to be registered.
  base::RunLoop run_loop;
  install_manager.on_register_previously_persisted_finished().Post(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  // Uninstall the extension.
  install_manager.Uninstall(kTestSystemExtensionId);
  TestExtensionUninstalled();
}

// Tests that if an extension is uninstalled, it stays uninstalled.
IN_PROC_BROWSER_TEST_F(SystemExtensionsBrowserTest,
                       PRE_PRE_PermanentlyUninstalled) {
  auto& provider = SystemExtensionsProvider::Get(browser()->profile());
  auto& install_manager = provider.install_manager();

  TestInstallationEventsWaiter waiter(provider);

  {
    // Install and wait for the service worker to be registered.
    base::RunLoop run_loop;
    install_manager.InstallUnpackedExtensionFromDir(
        GetBasicSystemExtensionDir(),
        base::BindLambdaForTesting(
            [&](InstallStatusOrSystemExtensionId result) { run_loop.Quit(); }));
    run_loop.Run();
    waiter.WaitForServiceWorkerRegistered();
  }
}

IN_PROC_BROWSER_TEST_F(SystemExtensionsBrowserTest,
                       PRE_PermanentlyUninstalled) {
  auto& provider = SystemExtensionsProvider::Get(browser()->profile());
  auto& install_manager = provider.install_manager();

  // Wait for previously persisted System Extensions to be registered.
  base::RunLoop run_loop;
  install_manager.on_register_previously_persisted_finished().Post(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  // Uninstall the extension.
  install_manager.Uninstall(kTestSystemExtensionId);

  TestExtensionUninstalled();
}

IN_PROC_BROWSER_TEST_F(SystemExtensionsBrowserTest, PermanentlyUninstalled) {
  auto& provider = SystemExtensionsProvider::Get(browser()->profile());
  auto& install_manager = provider.install_manager();

  // Wait for previously persisted System Extensions to be registered.
  base::RunLoop run_loop;
  install_manager.on_register_previously_persisted_finished().Post(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  TestExtensionUninstalled();
}

IN_PROC_BROWSER_TEST_F(SystemExtensionsSwitchBrowserTest, ExtensionInstalled) {
  auto& provider = SystemExtensionsProvider::Get(browser()->profile());
  auto& install_manager = provider.install_manager();

  base::RunLoop run_loop;
  install_manager.on_command_line_install_finished().Post(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  TestInstalledTestExtensionWorks();
}

IN_PROC_BROWSER_TEST_F(
    SystemExtensionsBrowserTestWithManagedDeviceHealthServicesPreTest,
    PRE_SystemExtensionsManagedDeviceHealthServices) {
  auto& provider = SystemExtensionsProvider::Get(browser()->profile());
  auto& install_manager = provider.install_manager();

  TestInstallationEventsWaiter waiter(provider);

  {
    // Install and wait for the service worker to be registered.
    base::RunLoop run_loop;
    install_manager.InstallUnpackedExtensionFromDir(
        GetManagedDeviceHealthServicesExtensionDir(),
        base::BindLambdaForTesting(
            [&](InstallStatusOrSystemExtensionId result) { run_loop.Quit(); }));
    run_loop.Run();
    waiter.WaitForServiceWorkerRegistered();
  }
}

IN_PROC_BROWSER_TEST_F(
    SystemExtensionsBrowserTestWithManagedDeviceHealthServicesPreTest,
    SystemExtensionsManagedDeviceHealthServices) {
  auto& provider = SystemExtensionsProvider::Get(browser()->profile());
  auto& install_manager = provider.install_manager();

  // Wait for previously persisted System Extensions to be registered.
  base::RunLoop run_loop;
  install_manager.on_register_previously_persisted_finished().Post(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  auto& registry = provider.registry();
  EXPECT_TRUE(registry.GetIds().empty());
  EXPECT_FALSE(registry.GetById(kTestSystemExtensionId));
}

}  // namespace ash
