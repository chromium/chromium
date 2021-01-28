// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/file_system_provider/notification_manager_interface.h"
#include "chrome/browser/chromeos/file_system_provider/observer.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/chromeos/file_system_provider/request_manager.h"
#include "chrome/browser/chromeos/file_system_provider/request_value.h"
#include "chrome/browser/chromeos/file_system_provider/service.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "content/public/test/browser_test.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace extensions {
namespace {

using chromeos::file_system_provider::MountContext;
using chromeos::file_system_provider::NotificationManagerInterface;
using chromeos::file_system_provider::Observer;
using chromeos::file_system_provider::ProvidedFileSystemInterface;
using chromeos::file_system_provider::ProvidedFileSystemInfo;
using chromeos::file_system_provider::RequestManager;
using chromeos::file_system_provider::RequestType;
using chromeos::file_system_provider::RequestValue;
using chromeos::file_system_provider::Service;

// Clicks the default button on the notification as soon as request timeouts
// and a unresponsiveness notification is shown.
class NotificationButtonClicker : public RequestManager::Observer {
 public:
  explicit NotificationButtonClicker(
      const ProvidedFileSystemInfo& file_system_info)
      : file_system_info_(file_system_info) {}
  ~NotificationButtonClicker() override {}

  // RequestManager::Observer overrides.
  void OnRequestCreated(int request_id, RequestType type) override {}
  void OnRequestDestroyed(int request_id) override {}
  void OnRequestExecuted(int request_id) override {}
  void OnRequestFulfilled(int request_id,
                          const RequestValue& result,
                          bool has_more) override {}
  void OnRequestRejected(int request_id,
                         const RequestValue& result,
                         base::File::Error error) override {}
  void OnRequestTimeouted(int request_id) override {
    // Call asynchronously so the notification is setup is completed.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&NotificationButtonClicker::ClickButton,
                                  base::Unretained(this)));
  }

 private:
  void ClickButton() {
    base::Optional<message_center::Notification> notification =
        NotificationDisplayServiceTester::Get()->GetNotification(
            file_system_info_.mount_path().value());
    if (notification)
      notification->delegate()->Click(0, base::nullopt);
  }

  ProvidedFileSystemInfo file_system_info_;

  DISALLOW_COPY_AND_ASSIGN(NotificationButtonClicker);
};

// Simulates clicking on the unresponsive notification's abort button. Also,
// sets the timeout delay to 0 ms, so the notification is shown faster.
class AbortOnUnresponsivePerformer : public Observer {
 public:
  explicit AbortOnUnresponsivePerformer(Profile* profile)
      : service_(Service::Get(profile)) {
    DCHECK(profile);
    DCHECK(service_);
    service_->AddObserver(this);
  }

  ~AbortOnUnresponsivePerformer() override { service_->RemoveObserver(this); }

  // Observer overrides.
  void OnProvidedFileSystemMount(const ProvidedFileSystemInfo& file_system_info,
                                 MountContext context,
                                 base::File::Error error) override {
    if (error != base::File::FILE_OK)
      return;

    ProvidedFileSystemInterface* const file_system =
        service_->GetProvidedFileSystem(file_system_info.provider_id(),
                                        file_system_info.file_system_id());
    DCHECK(file_system);
    file_system->GetRequestManager()->SetTimeoutForTesting(base::TimeDelta());

    std::unique_ptr<NotificationButtonClicker> clicker(
        new NotificationButtonClicker(file_system->GetFileSystemInfo()));

    file_system->GetRequestManager()->AddObserver(clicker.get());
    clickers_.push_back(std::move(clicker));
  }

  void OnProvidedFileSystemUnmount(
      const ProvidedFileSystemInfo& file_system_info,
      base::File::Error error) override {}

 private:
  Service* service_;  // Not owned.
  std::vector<std::unique_ptr<NotificationButtonClicker>> clickers_;

  DISALLOW_COPY_AND_ASSIGN(AbortOnUnresponsivePerformer);
};

}  // namespace

class FileSystemProviderApiTest : public ExtensionApiTest {
 public:
  FileSystemProviderApiTest() {}

  // Loads a helper testing extension.
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    const extensions::Extension* extension = LoadExtension(
        test_data_dir_.AppendASCII("file_system_provider/test_util"),
        {.allow_in_incognito = true});
    ASSERT_TRUE(extension);

    display_service_ = std::make_unique<NotificationDisplayServiceTester>(
        browser()->profile());

    user_manager_.AddUser(AccountId::FromUserEmailGaiaId(
        browser()->profile()->GetProfileUserName(), "12345"));
  }

  std::unique_ptr<NotificationDisplayServiceTester> display_service_;

 private:
  chromeos::FakeChromeUserManager user_manager_;
  DISALLOW_COPY_AND_ASSIGN(FileSystemProviderApiTest);
};

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, Mount) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags("file_system_provider/mount",
                                          kFlagNone, kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, Unmount) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags("file_system_provider/unmount",
                                          kFlagNone, kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, GetAll) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags("file_system_provider/get_all",
                                          kFlagNone, kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, GetMetadata) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags("file_system_provider/get_metadata",
                                          kFlagNone, kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, ReadDirectory) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags("file_system_provider/read_directory",
                                          kFlagNone, kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, ReadFile) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags("file_system_provider/read_file",
                                          kFlagNone, kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, BigFile) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags("file_system_provider/big_file",
                                          kFlagNone, kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, Evil) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags("file_system_provider/evil",
                                          kFlagNone, kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, MimeType) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags("file_system_provider/mime_type",
                                          kFlagNone, kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, CreateDirectory) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags(
      "file_system_provider/create_directory", kFlagNone, kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, DeleteEntry) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags("file_system_provider/delete_entry",
                                          kFlagNone, kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, CreateFile) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags("file_system_provider/create_file",
                                          kFlagNone, kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, CopyEntry) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags("file_system_provider/copy_entry",
                                          kFlagNone, kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, MoveEntry) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags("file_system_provider/move_entry",
                                          kFlagNone, kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, Truncate) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags("file_system_provider/truncate",
                                          kFlagNone, kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, WriteFile) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags("file_system_provider/write_file",
                                          kFlagNone, kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, Extension) {
  ASSERT_TRUE(RunComponentExtensionTest("file_system_provider/extension"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, Thumbnail) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags("file_system_provider/thumbnail",
                                          kFlagNone, kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, AddWatcher) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags("file_system_provider/add_watcher",
                                          kFlagNone, kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, RemoveWatcher) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags("file_system_provider/remove_watcher",
                                          kFlagNone, kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, Notify) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags("file_system_provider/notify",
                                          kFlagNone, kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, Configure) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags("file_system_provider/configure",
                                          kFlagNone, kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, GetActions) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags("file_system_provider/get_actions",
                                          kFlagNone, kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, ExecuteAction) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags("file_system_provider/execute_action",
                                          kFlagNone, kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, Unresponsive_Extension) {
  AbortOnUnresponsivePerformer performer(browser()->profile());
  ASSERT_TRUE(
      RunComponentExtensionTest("file_system_provider/unresponsive_extension"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, Unresponsive_App) {
  AbortOnUnresponsivePerformer performer(browser()->profile());
  ASSERT_TRUE(RunPlatformAppTestWithFlags(
      "file_system_provider/unresponsive_app", kFlagNone, kFlagLoadAsComponent))
      << message_;
}

}  // namespace extensions
