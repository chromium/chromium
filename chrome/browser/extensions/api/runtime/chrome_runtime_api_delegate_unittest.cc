// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/extensions/api/runtime/chrome_runtime_api_delegate.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_with_install.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/extensions/update_install_gate.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/updater/extension_downloader.h"
#include "extensions/browser/updater/extension_downloader_test_delegate.h"
#include "extensions/common/extension.h"
#include "extensions/common/verifier_formats.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

// A fake EventRouter that lets us pretend an extension has a listener
// registered for named events.
class TestEventRouter : public EventRouter {
 public:
  explicit TestEventRouter(content::BrowserContext* context)
      : EventRouter(context, ExtensionPrefs::Get(context)) {}
  ~TestEventRouter() override {}

  // An entry in our fake event registry.
  using Entry = std::pair<std::string, std::string>;

  bool ExtensionHasEventListener(const std::string& extension_id,
                                 const std::string& event_name) const override {
    return base::Contains(fake_registry_, Entry(extension_id, event_name));
  }

  // Pretend that |extension_id| is listening for |event_name|.
  void AddFakeListener(const std::string& extension_id,
                       const std::string& event_name) {
    fake_registry_.insert(Entry(extension_id, event_name));
  }

 private:
  std::set<Entry> fake_registry_;

  DISALLOW_COPY_AND_ASSIGN(TestEventRouter);
};

std::unique_ptr<KeyedService> TestEventRouterFactoryFunction(
    content::BrowserContext* context) {
  return std::make_unique<TestEventRouter>(context);
}

// This class lets us intercept extension update checks and respond as if
// either no update was found, or one was (and it was downloaded).
class DownloaderTestDelegate : public ExtensionDownloaderTestDelegate {
 public:
  DownloaderTestDelegate() {}

  // On the next update check for extension |id|, we'll respond that no update
  // is available.
  void AddNoUpdateResponse(const std::string& id) {
    no_updates_.insert(id);
    if (updates_.find(id) != updates_.end())
      updates_.erase(id);
  }

  // On the next update check for extension |id|, pretend that an update to
  // version |version| has been downloaded to |path|.
  void AddUpdateResponse(const std::string& id,
                         const base::FilePath& path,
                         const std::string& version) {
    if (no_updates_.find(id) != no_updates_.end())
      no_updates_.erase(id);
    DownloadFinishedArgs args;
    args.path = path;
    args.version = version;
    updates_[id] = std::move(args);
  }

  void StartUpdateCheck(
      ExtensionDownloader* downloader,
      ExtensionDownloaderDelegate* delegate,
      std::unique_ptr<ManifestFetchData> fetch_data) override {
    // Instead of immediately firing callbacks to the delegate in matching
    // cases below, we instead post a task since the delegate typically isn't
    // expecting a synchronous reply (the real code has to go do at least one
    // network request before getting a response, so this is is a reasonable
    // expectation by delegates).
    for (const std::string& id : fetch_data->extension_ids()) {
      auto no_update = no_updates_.find(id);
      if (no_update != no_updates_.end()) {
        no_updates_.erase(no_update);
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE,
            base::BindOnce(
                &ExtensionDownloaderDelegate::OnExtensionDownloadFailed,
                base::Unretained(delegate), id,
                ExtensionDownloaderDelegate::Error::NO_UPDATE_AVAILABLE,
                ExtensionDownloaderDelegate::PingResult(),
                fetch_data->request_ids()));
        continue;
      }
      auto update = updates_.find(id);
      if (update != updates_.end()) {
        CRXFileInfo info(id, update->second.path, "" /* no hash */,
                         GetTestVerifierFormat());
        std::string version = update->second.version;
        updates_.erase(update);
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE,
            base::BindOnce(
                &ExtensionDownloaderDelegate::OnExtensionDownloadFinished,
                base::Unretained(delegate), info,
                false /* file_ownership_passed */, GURL(), version,
                ExtensionDownloaderDelegate::PingResult(),
                fetch_data->request_ids(),
                ExtensionDownloaderDelegate::InstallCallback()));
        continue;
      }
      ADD_FAILURE() << "Unexpected extension id " << id;
    }
  }

 private:
  // Simple holder for the data passed in AddUpdateResponse calls.
  struct DownloadFinishedArgs {
    base::FilePath path;
    std::string version;
  };

  // These keep track of what response we should give for update checks, keyed
  // by extension id. A given extension id should only appear in one or the
  // other.
  std::set<std::string> no_updates_;
  std::map<std::string, DownloadFinishedArgs> updates_;

  DISALLOW_COPY_AND_ASSIGN(DownloaderTestDelegate);
};

// Helper to let test code wait for and return an update check result.
class UpdateCheckResultCatcher {
 public:
  UpdateCheckResultCatcher() {}

  void OnResult(const RuntimeAPIDelegate::UpdateCheckResult& result) {
    EXPECT_EQ(nullptr, result_.get());
    result_ = std::make_unique<RuntimeAPIDelegate::UpdateCheckResult>(
        result.success, result.response, result.version);
    if (run_loop_)
      run_loop_->Quit();
  }

  std::unique_ptr<RuntimeAPIDelegate::UpdateCheckResult> WaitForResult() {
    if (!result_) {
      run_loop_ = std::make_unique<base::RunLoop>();
      run_loop_->Run();
    }
    return std::move(result_);
  }

 private:
  std::unique_ptr<RuntimeAPIDelegate::UpdateCheckResult> result_;
  std::unique_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(UpdateCheckResultCatcher);
};

class ChromeRuntimeAPIDelegateTest : public ExtensionServiceTestWithInstall {
 public:
  ChromeRuntimeAPIDelegateTest() {}

  void SetUp() override {
    ExtensionServiceTestWithInstall::SetUp();
    ExtensionDownloader::set_test_delegate(&downloader_test_delegate_);
    ChromeRuntimeAPIDelegate::set_tick_clock_for_tests(&clock_);

    InitializeExtensionServiceWithUpdater();
    runtime_delegate_ =
        std::make_unique<ChromeRuntimeAPIDelegate>(browser_context());
    service()->updater()->SetExtensionCacheForTesting(nullptr);
    EventRouterFactory::GetInstance()->SetTestingFactory(
        browser_context(),
        base::BindRepeating(&TestEventRouterFactoryFunction));

    // Setup the ExtensionService so that extension updates won't complete
    // installation until the extension is idle.
    update_install_gate_ = std::make_unique<UpdateInstallGate>(service());
    service()->RegisterInstallGate(ExtensionPrefs::DELAY_REASON_WAIT_FOR_IDLE,
                                   update_install_gate_.get());
    static_cast<TestExtensionSystem*>(ExtensionSystem::Get(browser_context()))
        ->SetReady();
  }

  void TearDown() override {
    ExtensionDownloader::set_test_delegate(nullptr);
    ChromeRuntimeAPIDelegate::set_tick_clock_for_tests(nullptr);
    ExtensionServiceTestWithInstall::TearDown();
  }

  // Uses runtime_delegate_ to run an update check for |id|, expecting
  // |expected_response| and (if an update was available) |expected_version|.
  // The |expected_response| should be one of 'throttled', 'no_update', or
  // 'update_available'.
  void DoUpdateCheck(const std::string& id,
                     const std::string& expected_response,
                     const std::string& expected_version) {
    UpdateCheckResultCatcher catcher;
    EXPECT_TRUE(runtime_delegate_->CheckForUpdates(
        id, base::Bind(&UpdateCheckResultCatcher::OnResult,
                       base::Unretained(&catcher))));
    std::unique_ptr<RuntimeAPIDelegate::UpdateCheckResult> result =
        catcher.WaitForResult();
    ASSERT_NE(nullptr, result.get());
    EXPECT_TRUE(result->success);
    EXPECT_EQ(expected_response, result->response);
    EXPECT_EQ(expected_version, result->version);
  }

 protected:
  // A clock we pass to the code used for throttling, so that we can manually
  // increment time to test various throttling scenarios.
  base::SimpleTestTickClock clock_;

  // Used for intercepting update check requests and possibly returning fake
  // download results.
  DownloaderTestDelegate downloader_test_delegate_;

  // The object whose behavior we're testing.
  std::unique_ptr<RuntimeAPIDelegate> runtime_delegate_;

  // For preventing extensions from being updated immediately.
  std::unique_ptr<UpdateInstallGate> update_install_gate_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeRuntimeAPIDelegateTest);
};

TEST_F(ChromeRuntimeAPIDelegateTest, RequestUpdateCheck) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath root_dir = data_dir().AppendASCII("autoupdate");
  base::FilePath pem_path = root_dir.AppendASCII("key.pem");
  base::FilePath v1_path = temp_dir.GetPath().AppendASCII("v1.crx");
  base::FilePath v2_path = temp_dir.GetPath().AppendASCII("v2.crx");
  PackCRX(root_dir.AppendASCII("v1"), pem_path, v1_path);
  PackCRX(root_dir.AppendASCII("v2"), pem_path, v2_path);

  // Start by installing version 1.
  scoped_refptr<const Extension> v1(InstallCRX(v1_path, INSTALL_NEW));
  std::string id = v1->id();

  // Make it look like our test extension listens for the
  // runtime.onUpdateAvailable event, so that it won't be updated immediately
  // when the ExtensionUpdater hands the new version to the ExtensionService.
  TestEventRouter* event_router =
      static_cast<TestEventRouter*>(EventRouter::Get(browser_context()));
  event_router->AddFakeListener(id, "runtime.onUpdateAvailable");

  // Run an update check that should get a "no_update" response.
  downloader_test_delegate_.AddNoUpdateResponse(id);
  DoUpdateCheck(id, "no_update", "");

  // Check again after a short delay - we should be throttled because
  // not enough time has passed.
  clock_.Advance(base::TimeDelta::FromMinutes(15));
  downloader_test_delegate_.AddNoUpdateResponse(id);
  DoUpdateCheck(id, "throttled", "");

  // Now simulate checking a few times at a 6 hour interval - none of these
  // should be throttled.
  for (int i = 0; i < 5; i++) {
    clock_.Advance(base::TimeDelta::FromHours(6));
    downloader_test_delegate_.AddNoUpdateResponse(id);
    DoUpdateCheck(id, "no_update", "");
  }

  // Run an update check that should get an "update_available" response. This
  // actually causes the new version to be downloaded/unpacked, but the install
  // will not complete until we reload the extension.
  clock_.Advance(base::TimeDelta::FromDays(1));
  downloader_test_delegate_.AddUpdateResponse(id, v2_path, "2.0");
  DoUpdateCheck(id, "update_available", "2.0");

  // Call again after short delay - it should be throttled instead of getting
  // another "update_available" response.
  clock_.Advance(base::TimeDelta::FromMinutes(30));
  downloader_test_delegate_.AddUpdateResponse(id, v2_path, "2.0");
  DoUpdateCheck(id, "throttled", "");

  // Reload the extension, causing the delayed update to v2 to happen, then do
  // another update check - we should get a no_update instead of throttled.
  service()->ReloadExtension(id);
  const Extension* current =
      ExtensionRegistry::Get(browser_context())->GetInstalledExtension(id);
  ASSERT_NE(nullptr, current);
  EXPECT_EQ("2.0", current->VersionString());
  clock_.Advance(base::TimeDelta::FromSeconds(10));
  downloader_test_delegate_.AddNoUpdateResponse(id);
  DoUpdateCheck(id, "no_update", "");

  // Check again after short delay; we should be throttled.
  clock_.Advance(base::TimeDelta::FromMinutes(5));
  DoUpdateCheck(id, "throttled", "");

  // Call again after a longer delay, we should should be unthrottled.
  clock_.Advance(base::TimeDelta::FromHours(8));
  downloader_test_delegate_.AddNoUpdateResponse(id);
  DoUpdateCheck(id, "no_update", "");
}

class ExtensionLoadWaiter : public ExtensionRegistryObserver {
 public:
  explicit ExtensionLoadWaiter(content::BrowserContext* context)
      : context_(context) {
    extension_registry_observer_.Add(ExtensionRegistry::Get(context_));
  }

  void WaitForReload() { run_loop_.Run(); }

 protected:
  // ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override {
    // We unblock the test every time the extension has been reloaded.
    run_loop_.Quit();
  }

  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override {
    // This gets triggered before OnExtensionLoaded. If an extension is being
    // reloaded, we wait until OnExtensionLoaded to unblock the test to reload
    // again. Otherwise, a race condition occurs where the test tries to reload
    // the extension while it's disabled in between reloads. We unblock the test
    // if the extension gets terminated as this will be the last lifecycle
    // method called.
    if (reason == UnloadedExtensionReason::TERMINATE) {
      run_loop_.Quit();
    }
  }

 private:
  base::RunLoop run_loop_;
  content::BrowserContext* context_;
  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(ExtensionLoadWaiter);
};

class ChromeRuntimeAPIDelegateReloadTest : public ChromeRuntimeAPIDelegateTest {
 public:
  ChromeRuntimeAPIDelegateReloadTest() {}

  void SetUp() override {
    ChromeRuntimeAPIDelegateTest::SetUp();

    ChromeTestExtensionLoader loader(browser_context());
    scoped_refptr<const Extension> extension =
        loader.LoadExtension(data_dir().AppendASCII("common/background_page"));

    extension_id_ = extension->id();
  }

 protected:
  void ReloadExtensionAndWait() {
    ExtensionLoadWaiter waiter(browser_context());
    runtime_delegate_->ReloadExtension(extension_id_);
    waiter.WaitForReload();
  }

  ExtensionId extension_id() const { return extension_id_; }

 private:
  ExtensionId extension_id_;

  DISALLOW_COPY_AND_ASSIGN(ChromeRuntimeAPIDelegateReloadTest);
};

TEST_F(ChromeRuntimeAPIDelegateReloadTest,
       TerminateExtensionWithTooManyReloads) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  // We expect the extension to be reloaded 30 times in quick succession before
  // the next reload goes over the threshold for an unpacked extension and
  // causes it to terminate.
  const int kNumReloadsBeforeDisable = 30;
  clock_.SetNowTicks(base::TimeTicks::Now());
  for (int i = 0; i < kNumReloadsBeforeDisable; i++) {
    ReloadExtensionAndWait();
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id()));
  }

  // The 31st reload should terminate the extension.
  ReloadExtensionAndWait();
  EXPECT_TRUE(registry()->terminated_extensions().Contains(extension_id()));
}

TEST_F(ChromeRuntimeAPIDelegateReloadTest,
       ReloadExtensionAfterThresholdInterval) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  // Reload only thirty times, as we will pause until the fast reload threshold
  // has passed before reloading again.
  const int kNumReloads = 30;

  clock_.SetNowTicks(base::TimeTicks::Now());
  for (int i = 0; i < kNumReloads; i++) {
    ReloadExtensionAndWait();
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id()));
  }

  // Reload one more time after the time threshold for a suspiciously fast
  // reload has passed.
  clock_.Advance(base::TimeDelta::FromSeconds(1000));

  ReloadExtensionAndWait();
  EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id()));
}

}  // namespace

}  // namespace extensions
