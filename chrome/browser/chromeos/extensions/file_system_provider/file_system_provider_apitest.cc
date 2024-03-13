// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include "build/build_config.h"

#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/file_system_provider/observer.h"
#include "chrome/browser/ash/file_system_provider/operation_request_manager.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/ash/file_system_provider/request_manager.h"
#include "chrome/browser/ash/file_system_provider/request_value.h"
#include "chrome/browser/ash/file_system_provider/service.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/ui/browser.h"
#include "content/public/test/browser_test.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace extensions {
namespace {

using ash::file_system_provider::MountContext;
using ash::file_system_provider::Observer;
using ash::file_system_provider::OperationCompletion;
using ash::file_system_provider::ProvidedFileSystemInfo;
using ash::file_system_provider::ProvidedFileSystemInterface;
using ash::file_system_provider::RequestManager;
using ash::file_system_provider::RequestType;
using ash::file_system_provider::RequestValue;
using ash::file_system_provider::Service;

// Clicks the default button on the notification as soon as request timeouts
// and a unresponsiveness notification is shown.
class NotificationButtonClicker : public RequestManager::Observer {
 public:
  explicit NotificationButtonClicker(
      const ProvidedFileSystemInfo& file_system_info)
      : file_system_info_(file_system_info) {}

  NotificationButtonClicker(const NotificationButtonClicker&) = delete;
  NotificationButtonClicker& operator=(const NotificationButtonClicker&) =
      delete;

  ~NotificationButtonClicker() override {}

  // RequestManager::Observer overrides.
  void OnRequestCreated(int request_id, RequestType type) override {}
  void OnRequestDestroyed(int request_id,
                          OperationCompletion completion) override {}
  void OnRequestExecuted(int request_id) override {}
  void OnRequestFulfilled(int request_id,
                          const RequestValue& result,
                          bool has_more) override {}
  void OnRequestRejected(int request_id,
                         const RequestValue& result,
                         base::File::Error error) override {}
  void OnRequestTimedOut(int request_id) override {
    // Call asynchronously so the notification is setup is completed.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&NotificationButtonClicker::ClickButton,
                                  base::Unretained(this)));
  }

 private:
  void ClickButton() {
    std::optional<message_center::Notification> notification =
        NotificationDisplayServiceTester::Get()->GetNotification(
            file_system_info_.mount_path().value());
    if (notification)
      notification->delegate()->Click(0, std::nullopt);
  }

  ProvidedFileSystemInfo file_system_info_;
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

  AbortOnUnresponsivePerformer(const AbortOnUnresponsivePerformer&) = delete;
  AbortOnUnresponsivePerformer& operator=(const AbortOnUnresponsivePerformer&) =
      delete;

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
  raw_ptr<Service> service_;  // Not owned.
  std::vector<std::unique_ptr<NotificationButtonClicker>> clickers_;
};

}  // namespace

class FileSystemProviderApiTest : public ExtensionApiTest {
 public:
  FileSystemProviderApiTest() {}

  FileSystemProviderApiTest(const FileSystemProviderApiTest&) = delete;
  FileSystemProviderApiTest& operator=(const FileSystemProviderApiTest&) =
      delete;

  // Loads a helper testing extension.
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    const extensions::Extension* extension = LoadExtension(
        test_data_dir_.AppendASCII("file_system_provider/test_util"),
        {.allow_in_incognito = true});
    ASSERT_TRUE(extension);

    display_service_ = std::make_unique<NotificationDisplayServiceTester>(
        browser()->profile());

    user_manager_.AddUser(AccountId::FromUserEmailGaiaId("test@test", "12345"));
  }

  std::unique_ptr<NotificationDisplayServiceTester> display_service_;

 private:
  ash::FakeChromeUserManager user_manager_;
};

using FileSystemProviderServiceWorkerApiTest = FileSystemProviderApiTest;

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, Mount) {
  ASSERT_TRUE(RunExtensionTest("file_system_provider/mount",
                               {.launch_as_platform_app = true},
                               {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, Unmount) {
  ASSERT_TRUE(RunExtensionTest("file_system_provider/unmount",
                               {.launch_as_platform_app = true},
                               {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, GetAll) {
  ASSERT_TRUE(RunExtensionTest("file_system_provider/get_all",
                               {.launch_as_platform_app = true},
                               {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, GetMetadata) {
  ASSERT_TRUE(RunExtensionTest("file_system_provider/get_metadata",
                               {.launch_as_platform_app = true},
                               {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, ReadDirectory) {
  ASSERT_TRUE(RunExtensionTest("file_system_provider/read_directory",
                               {.launch_as_platform_app = true},
                               {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, ReadFile) {
  ASSERT_TRUE(RunExtensionTest("file_system_provider/read_file",
                               {.launch_as_platform_app = true},
                               {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, BigFile) {
  ASSERT_TRUE(RunExtensionTest("file_system_provider/big_file",
                               {.launch_as_platform_app = true},
                               {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, Evil) {
  ASSERT_TRUE(RunExtensionTest("file_system_provider/evil",
                               {.launch_as_platform_app = true},
                               {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, MimeType) {
  ASSERT_TRUE(RunExtensionTest("file_system_provider/mime_type",
                               {.launch_as_platform_app = true},
                               {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, CreateDirectory) {
  ASSERT_TRUE(RunExtensionTest("file_system_provider/create_directory",
                               {.launch_as_platform_app = true},
                               {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, DeleteEntry) {
  ASSERT_TRUE(RunExtensionTest("file_system_provider/delete_entry",
                               {.launch_as_platform_app = true},
                               {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, CreateFile) {
  ASSERT_TRUE(RunExtensionTest("file_system_provider/create_file",
                               {.launch_as_platform_app = true},
                               {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, CopyEntry) {
  ASSERT_TRUE(RunExtensionTest("file_system_provider/copy_entry",
                               {.launch_as_platform_app = true},
                               {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, MoveEntry) {
  ASSERT_TRUE(RunExtensionTest("file_system_provider/move_entry",
                               {.launch_as_platform_app = true},
                               {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, Truncate) {
  ASSERT_TRUE(RunExtensionTest("file_system_provider/truncate",
                               {.launch_as_platform_app = true},
                               {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, WriteFile) {
  ASSERT_TRUE(RunExtensionTest("file_system_provider/write_file",
                               {.launch_as_platform_app = true},
                               {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, Extension) {
  ASSERT_TRUE(RunExtensionTest("file_system_provider/extension", {},
                               {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, Thumbnail) {
  ASSERT_TRUE(RunExtensionTest("file_system_provider/thumbnail",
                               {.launch_as_platform_app = true},
                               {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, AddWatcher) {
  ASSERT_TRUE(RunExtensionTest("file_system_provider/add_watcher",
                               {.launch_as_platform_app = true},
                               {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, RemoveWatcher) {
  ASSERT_TRUE(RunExtensionTest("file_system_provider/remove_watcher",
                               {.launch_as_platform_app = true},
                               {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, Notify) {
  ASSERT_TRUE(RunExtensionTest("file_system_provider/notify",
                               {.launch_as_platform_app = true},
                               {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, Configure) {
  ASSERT_TRUE(RunExtensionTest("file_system_provider/configure",
                               {.launch_as_platform_app = true},
                               {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, GetActions) {
  ASSERT_TRUE(RunExtensionTest("file_system_provider/get_actions",
                               {.launch_as_platform_app = true},
                               {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, ExecuteAction) {
  ASSERT_TRUE(RunExtensionTest("file_system_provider/execute_action",
                               {.launch_as_platform_app = true},
                               {.load_as_component = true}))
      << message_;
}

// TODO(b/255698656): Flaky test.
IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest,
                       DISABLED_Unresponsive_Extension) {
  AbortOnUnresponsivePerformer performer(browser()->profile());
  ASSERT_TRUE(RunExtensionTest("file_system_provider/unresponsive_extension",
                               {}, {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, Unresponsive_App) {
  AbortOnUnresponsivePerformer performer(browser()->profile());
  ASSERT_TRUE(RunExtensionTest("file_system_provider/unresponsive_app",
                               {.launch_as_platform_app = true},
                               {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderServiceWorkerApiTest, AddWatcher) {
  ASSERT_TRUE(RunExtensionTest(
      "file_system_provider/service_worker/add_watcher",
      {.extension_url = "test.html"}, {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderServiceWorkerApiTest, BigFile) {
  ASSERT_TRUE(RunExtensionTest("file_system_provider/service_worker/big_file",
                               {.extension_url = "test.html"},
                               {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderServiceWorkerApiTest, Configure) {
  ASSERT_TRUE(RunExtensionTest("file_system_provider/service_worker/configure",
                               {.extension_url = "test.html"},
                               {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderServiceWorkerApiTest, CopyEntry) {
  ASSERT_TRUE(RunExtensionTest("file_system_provider/service_worker/copy_entry",
                               {.extension_url = "test.html"},
                               {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderServiceWorkerApiTest,
                       CreateDirectory) {
  ASSERT_TRUE(RunExtensionTest(
      "file_system_provider/service_worker/create_directory",
      {.extension_url = "test.html"}, {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderServiceWorkerApiTest, DeleteEntry) {
  ASSERT_TRUE(RunExtensionTest(
      "file_system_provider/service_worker/delete_entry",
      {.extension_url = "test.html"}, {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderServiceWorkerApiTest, Evil) {
  ASSERT_TRUE(RunExtensionTest("file_system_provider/service_worker/evil",
                               {.extension_url = "test.html"},
                               {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderServiceWorkerApiTest, ExecuteAction) {
  ASSERT_TRUE(RunExtensionTest(
      "file_system_provider/service_worker/execute_action",
      {.extension_url = "test.html"}, {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderServiceWorkerApiTest, GetActions) {
  ASSERT_TRUE(RunExtensionTest(
      "file_system_provider/service_worker/get_actions",
      {.extension_url = "test.html"}, {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderServiceWorkerApiTest, GetAll) {
  ASSERT_TRUE(RunExtensionTest("file_system_provider/service_worker/get_all",
                               {.extension_url = "test.html"},
                               {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderServiceWorkerApiTest, GetMetadata) {
  ASSERT_TRUE(RunExtensionTest(
      "file_system_provider/service_worker/get_metadata",
      {.extension_url = "test.html"}, {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderServiceWorkerApiTest, MimeType) {
  // Install a Chrome app that handles our custom MIME type.
  LoadExtension(test_data_dir_.AppendASCII(
                    "file_system_provider/service_worker/mime_type/app"),
                {.allow_in_incognito = true});

  ASSERT_TRUE(RunExtensionTest("file_system_provider/service_worker/mime_type",
                               {.extension_url = "test.html"},
                               {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderServiceWorkerApiTest, Mount) {
  ASSERT_TRUE(RunExtensionTest("file_system_provider/service_worker/mount",
                               {.extension_url = "test.html"},
                               {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderServiceWorkerApiTest, MoveEntry) {
  ASSERT_TRUE(RunExtensionTest("file_system_provider/service_worker/move_entry",
                               {.extension_url = "test.html"},
                               {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderServiceWorkerApiTest, Notify) {
  ASSERT_TRUE(RunExtensionTest("file_system_provider/service_worker/notify",
                               {.extension_url = "test.html"},
                               {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderServiceWorkerApiTest, ReadDirectory) {
  ASSERT_TRUE(RunExtensionTest(
      "file_system_provider/service_worker/read_directory",
      {.extension_url = "test.html"}, {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderServiceWorkerApiTest, ReadFile) {
  ASSERT_TRUE(RunExtensionTest("file_system_provider/service_worker/read_file",
                               {.extension_url = "test.html"},
                               {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderServiceWorkerApiTest, RemoveWatcher) {
  ASSERT_TRUE(RunExtensionTest(
      "file_system_provider/service_worker/remove_watcher",
      {.extension_url = "test.html"}, {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderServiceWorkerApiTest, Thumbnail) {
  ASSERT_TRUE(RunExtensionTest("file_system_provider/service_worker/thumbnail",
                               {.extension_url = "test.html"},
                               {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderServiceWorkerApiTest, Truncate) {
  ASSERT_TRUE(RunExtensionTest("file_system_provider/service_worker/truncate",
                               {.extension_url = "test.html"},
                               {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderServiceWorkerApiTest, Unmount) {
  ASSERT_TRUE(RunExtensionTest("file_system_provider/service_worker/unmount",
                               {.extension_url = "test.html"},
                               {.load_as_component = true}))
      << message_;
}

#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_Unresponsive_Extension DISABLED_Unresponsive_Extension
#else
#define MAYBE_Unresponsive_Extension Unresponsive_Extension
#endif
IN_PROC_BROWSER_TEST_F(FileSystemProviderServiceWorkerApiTest,
                       MAYBE_Unresponsive_Extension) {
  AbortOnUnresponsivePerformer performer(browser()->profile());
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "file_system_provider/service_worker/unresponsive_extension/provider")));
  ASSERT_TRUE(RunExtensionTest(
      "file_system_provider/service_worker/unresponsive_extension",
      {.extension_url = "test.html"}, {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderServiceWorkerApiTest, WriteFile) {
  ASSERT_TRUE(RunExtensionTest("file_system_provider/service_worker/write_file",
                               {.extension_url = "test.html"},
                               {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderServiceWorkerApiTest, CreateFile) {
  ASSERT_TRUE(RunExtensionTest(
      "file_system_provider/service_worker/create_file",
      {.extension_url = "test.html"}, {.load_as_component = true}))
      << message_;
}

}  // namespace extensions
