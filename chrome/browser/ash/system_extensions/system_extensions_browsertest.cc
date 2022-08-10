// Copyright 2021 The Chromium Authors. All rights reserved.
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
#include "chrome/browser/ash/system_extensions/system_extensions_profile_utils.h"
#include "chrome/browser/ash/system_extensions/system_extensions_provider.h"
#include "chrome/browser/ash/system_extensions/system_extensions_provider_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/common/page_type.h"
#include "content/public/test/browser_test.h"

namespace ash {

namespace {

constexpr SystemExtensionId kTestSystemExtensionId = {1, 2, 3, 4};

constexpr char kTestSystemExtensionIndexURL[] =
    "chrome-untrusted://system-extension-echo-01020304/html/index.html";

constexpr char kTestSystemExtensionWrongURL[] =
    "chrome-untrusted://system-extension-echo-01020304/html/wrong.html";

constexpr char kTestSystemExtensionEmptyPathURL[] =
    "chrome-untrusted://system-extension-echo-01020304/";

base::FilePath GetBasicSystemExtensionDir() {
  base::FilePath test_dir;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_dir);
  return test_dir.Append("system_extensions").Append("basic_system_extension");
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

// Class that can be used to wait for SystemExtensionsInstallManager events.
// If an event is triggered more than once, this class will CHECK.
class TestInstallationEventsWaiter
    : public SystemExtensionsInstallManager::Observer {
 public:
  explicit TestInstallationEventsWaiter(SystemExtensionsProvider& provider)
      : install_manager_(provider.install_manager()) {
    observation_.Observe(&install_manager_);
  }

  ~TestInstallationEventsWaiter() override = default;

  // Returns the result of a Service Worker registration. Waits if there hasn't
  // been one yet.
  std::pair<SystemExtensionId, blink::ServiceWorkerStatusCode>
  WaitForServiceWorkerRegistered() {
    absl::optional<SystemExtensionId> id;
    absl::optional<blink::ServiceWorkerStatusCode> status_code;

    base::RunLoop run_loop;
    on_service_worker_registered_.Post(
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
    on_service_worker_unregistered_.Post(
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

  // SystemExtensionsInstallManager::Observer
  void OnServiceWorkerRegistered(
      const SystemExtensionId& id,
      blink::ServiceWorkerStatusCode status_code) override {
    on_service_worker_registered_.Signal(id, status_code);
  }

  void OnServiceWorkerUnregistered(const SystemExtensionId& id,
                                   bool succeeded) override {
    on_service_worker_unregistered_.Signal(id, succeeded);
  }

  void OnSystemExtensionAssetsDeleted(const SystemExtensionId& id,
                                      bool succeeded) override {
    on_assets_deleted_.Signal(id, succeeded);
  }

 private:
  OneShotEventWrapper<SystemExtensionId, blink::ServiceWorkerStatusCode>
      on_service_worker_registered_;
  OneShotEventWrapper<SystemExtensionId, bool> on_service_worker_unregistered_;
  OneShotEventWrapper<SystemExtensionId, bool> on_assets_deleted_;

  base::ScopedObservation<SystemExtensionsInstallManager,
                          SystemExtensionsInstallManager::Observer>
      observation_{this};

  // Should be present for the duration of the test.
  SystemExtensionsInstallManager& install_manager_;
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

  auto& registry = provider.registry();
  EXPECT_TRUE(registry.GetIds().empty());
  EXPECT_FALSE(registry.GetById(kTestSystemExtensionId));

  // Test that navigating to the System Extension's resources fails.
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(kTestSystemExtensionIndexURL)));
  content::NavigationEntry* entry = tab->GetController().GetVisibleEntry();
  EXPECT_EQ(content::PAGE_TYPE_ERROR, entry->GetPageType());

  {
    // Test that the resources have been deleted.
    const auto [id, deletion_succeeded] = waiter.WaitForAssetsDeleted();
    EXPECT_EQ(id, kTestSystemExtensionId);
    EXPECT_TRUE(deletion_succeeded);
    base::ScopedAllowBlockingForTesting allow_blocking;
    const base::FilePath system_extension_dir = GetDirectoryForSystemExtension(
        *browser()->profile(), kTestSystemExtensionId);
    EXPECT_FALSE(base::PathExists(system_extension_dir));
  }

  {
    // Test that the service worker has been unregistered.
    const auto [id, unregistration_succeeded] =
        waiter.WaitForServiceWorkerUnregistered();
    EXPECT_EQ(id, kTestSystemExtensionId);
    EXPECT_TRUE(unregistration_succeeded);

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

IN_PROC_BROWSER_TEST_F(SystemExtensionsSwitchBrowserTest, ExtensionInstalled) {
  auto& provider = SystemExtensionsProvider::Get(browser()->profile());
  auto& install_manager = provider.install_manager();

  base::RunLoop run_loop;
  install_manager.on_command_line_install_finished().Post(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  TestInstalledTestExtensionWorks();
}

}  // namespace ash
