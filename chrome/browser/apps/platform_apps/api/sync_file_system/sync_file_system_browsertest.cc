// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine.h"
#include "chrome/browser/sync_file_system/local/local_file_sync_service.h"
#include "chrome/browser/sync_file_system/sync_file_system_service.h"
#include "chrome/browser/sync_file_system/sync_file_system_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "components/drive/service/fake_drive_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "storage/browser/quota/quota_manager.h"
#include "third_party/leveldatabase/leveldb_chrome.h"

using signin::PrimaryAccountChangeEvent;

namespace sync_file_system {

namespace {

const char kEmail[] = "email@example.com";

class FakeDriveServiceFactory
    : public drive_backend::SyncEngine::DriveServiceFactory {
 public:
  explicit FakeDriveServiceFactory(
      drive::FakeDriveService::ChangeObserver* change_observer)
      : change_observer_(change_observer) {}
  FakeDriveServiceFactory(const FakeDriveServiceFactory&) = delete;
  FakeDriveServiceFactory& operator=(const FakeDriveServiceFactory&) = delete;
  ~FakeDriveServiceFactory() override = default;

  std::unique_ptr<drive::DriveServiceInterface> CreateDriveService(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      base::SequencedTaskRunner* blocking_task_runner) override {
    std::unique_ptr<drive::FakeDriveService> drive_service(
        new drive::FakeDriveService);
    drive_service->AddChangeObserver(change_observer_);
    return std::move(drive_service);
  }

 private:
  raw_ptr<drive::FakeDriveService::ChangeObserver> change_observer_;
};

}  // namespace

class SyncFileSystemTest : public extensions::PlatformAppBrowserTest,
                           public drive::FakeDriveService::ChangeObserver {
 public:
  SyncFileSystemTest() = default;
  SyncFileSystemTest(const SyncFileSystemTest&) = delete;
  SyncFileSystemTest& operator=(const SyncFileSystemTest&) = delete;

  scoped_refptr<base::SequencedTaskRunner> MakeSequencedTaskRunner() {
    return base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  }

  void SetUpOnMainThread() override {
    in_memory_env_ = leveldb_chrome::NewMemEnv("SyncFileSystemTest");
    extensions::PlatformAppBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(base_dir_.CreateUniqueTempDir());

    identity_test_env_ = std::make_unique<signin::IdentityTestEnvironment>();

    // Override factory to inject a test RemoteFileSyncService.
    SyncFileSystemServiceFactory::GetInstance()->SetTestingFactory(
        profile(),
        base::BindLambdaForTesting([this](content::BrowserContext* context)
                                       -> std::unique_ptr<KeyedService> {
          auto remote_service = base::WrapUnique(new drive_backend::SyncEngine(
              base::SingleThreadTaskRunner::GetCurrentDefault(),
              MakeSequencedTaskRunner(), MakeSequencedTaskRunner(),
              base_dir_.GetPath(),
              /*task_logger=*/nullptr,
              /*notification_manager=*/nullptr,
              extensions::ExtensionSystem::Get(context)->extension_service(),
              extensions::ExtensionRegistry::Get(context),
              identity_test_env_->identity_manager(),
              /*url_loader_factory=*/nullptr,
              std::make_unique<FakeDriveServiceFactory>(this),
              in_memory_env_.get()));
          remote_service->SetSyncEnabled(true);

          return SyncFileSystemServiceFactory::
              BuildWithRemoteFileSyncServiceForTest(context,
                                                    std::move(remote_service));
        }));
  }

  // drive::FakeDriveService::ChangeObserver override.
  void OnNewChangeAvailable() override {
    sync_engine()->OnNotificationTimerFired();
  }

  SyncFileSystemService* sync_file_system_service() {
    return SyncFileSystemServiceFactory::GetForProfile(browser()->profile());
  }

  drive_backend::SyncEngine* sync_engine() {
    return static_cast<drive_backend::SyncEngine*>(
        sync_file_system_service()->remote_service_.get());
  }

  LocalFileSyncService* local_file_sync_service() {
    return sync_file_system_service()->local_service_.get();
  }

  void SignIn() {
    identity_test_env_->SetPrimaryAccount(kEmail, signin::ConsentLevel::kSync);
  }

  void SetSyncEnabled(bool enabled) {
    sync_file_system_service()->SetSyncEnabledForTesting(enabled);
  }

  void WaitUntilIdle() {
    base::RunLoop run_loop;
    sync_file_system_service()->CallOnIdleForTesting(run_loop.QuitClosure());
    run_loop.Run();
  }

  signin::IdentityManager* identity_manager() const {
    return identity_test_env_->identity_manager();
  }

 private:
  base::ScopedTempDir base_dir_;
  std::unique_ptr<leveldb::Env> in_memory_env_;

  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_env_;
};

IN_PROC_BROWSER_TEST_F(SyncFileSystemTest, AuthorizationTest) {
  ExtensionTestMessageListener open_failure("checkpoint: Failed to get syncfs",
                                            ReplyBehavior::kWillReply);
  ExtensionTestMessageListener bar_created("checkpoint: \"/bar\" created",
                                           ReplyBehavior::kWillReply);
  ExtensionTestMessageListener foo_created("checkpoint: \"/foo\" created",
                                           ReplyBehavior::kWillReply);
  extensions::ResultCatcher catcher;

  LoadAndLaunchPlatformApp("sync_file_system/authorization_test", "Launched");

  // Application sync is disabled at the initial state.  Thus first
  // syncFilesystem.requestFileSystem call should fail.
  ASSERT_TRUE(open_failure.WaitUntilSatisfied());

  // Enable Application sync and let the app retry.
  SignIn();
  SetSyncEnabled(true);

  open_failure.Reply("resume");

  ASSERT_TRUE(foo_created.WaitUntilSatisfied());

  // The app creates a file "/foo", that should successfully sync to the remote
  // service.  Wait for the completion and resume the app.
  WaitUntilIdle();

  sync_engine()->OnPrimaryAccountChanged(
      PrimaryAccountChangeEvent(PrimaryAccountChangeEvent::State(
                                    identity_manager()->GetPrimaryAccountInfo(
                                        signin::ConsentLevel::kSync),
                                    signin::ConsentLevel::kSync),
                                PrimaryAccountChangeEvent::State(),
                                signin_metrics::ProfileSignout::kTest));
  foo_created.Reply("resume");

  ASSERT_TRUE(bar_created.WaitUntilSatisfied());

  // The app creates another file "/bar".  Since the user signed out from chrome
  // The synchronization should fail and the service state should be
  // AUTHENTICATION_REQUIRED.

  WaitUntilIdle();
  EXPECT_EQ(REMOTE_SERVICE_AUTHENTICATION_REQUIRED,
            sync_engine()->GetCurrentState());

  sync_engine()->OnPrimaryAccountChanged(PrimaryAccountChangeEvent(
      PrimaryAccountChangeEvent::State(),
      PrimaryAccountChangeEvent::State(
          identity_manager()->GetPrimaryAccountInfo(
              signin::ConsentLevel::kSync),
          signin::ConsentLevel::kSync),
      signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN));
  WaitUntilIdle();

  bar_created.Reply("resume");

  EXPECT_TRUE(catcher.GetNextResult());
}

}  // namespace sync_file_system
