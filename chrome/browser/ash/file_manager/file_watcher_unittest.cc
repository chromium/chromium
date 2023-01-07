// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/file_watcher.h"

#include <memory>
#include <utility>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_service.pb.h"
#include "chromeos/ash/components/dbus/cicerone/fake_cicerone_client.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/seneschal/seneschal_client.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/extension.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace file_manager {
namespace {

class FileManagerFileWatcherTest : public testing::Test {
 public:
  // Use IO_MAINLOOP so FilePathWatcher works in the fake FILE thread, which
  // is actually shared with the main thread.
  FileManagerFileWatcherTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {
    ash::CiceroneClient::InitializeFake();
    ash::ConciergeClient::InitializeFake();
    ash::SeneschalClient::InitializeFake();
  }

  ~FileManagerFileWatcherTest() override {
    ash::SeneschalClient::Shutdown();
    ash::ConciergeClient::Shutdown();
    ash::CiceroneClient::Shutdown();
  }

  void SetUp() override { profile_ = std::make_unique<TestingProfile>(); }

  void TearDown() override { profile_.reset(); }

  void FlushMessageLoopTasks() { task_environment_.RunUntilIdle(); }
  Profile* profile() { return profile_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(FileManagerFileWatcherTest, AddAndRemoveOneExtensionId) {
  const base::FilePath kVirtualPath =
      base::FilePath::FromUTF8Unsafe("foo/bar.txt");
  const char kExtensionId[] = "extension-id";
  GURL source_url =
      extensions::Extension::GetBaseURLFromExtensionId(kExtensionId);
  url::Origin origin = url::Origin::Create(source_url);

  FileWatcher file_watcher(kVirtualPath);
  file_watcher.AddListener(origin);
  std::vector<url::Origin> origins = file_watcher.GetListeners();

  ASSERT_EQ(1U, origins.size());
  ASSERT_EQ(origin, origins[0]);

  file_watcher.RemoveListener(origin);
  origins = file_watcher.GetListeners();
  ASSERT_EQ(0U, origins.size());
}

TEST_F(FileManagerFileWatcherTest, AddAndRemoveMultipleExtensionIds) {
  const base::FilePath kVirtualPath =
      base::FilePath::FromUTF8Unsafe("foo/bar.txt");
  const char kExtensionFooId[] = "extension-foo-id";
  const char kExtensionBarId[] = "extension-bar-id";
  GURL foo_source_url =
      extensions::Extension::GetBaseURLFromExtensionId(kExtensionFooId);
  url::Origin foo_origin = url::Origin::Create(foo_source_url);
  GURL bar_source_url =
      extensions::Extension::GetBaseURLFromExtensionId(kExtensionBarId);
  url::Origin bar_origin = url::Origin::Create(bar_source_url);

  FileWatcher file_watcher(kVirtualPath);
  file_watcher.AddListener(foo_origin);
  file_watcher.AddListener(bar_origin);
  std::vector<url::Origin> origins = file_watcher.GetListeners();

  // The list should be sorted.
  ASSERT_EQ(2U, origins.size());
  ASSERT_EQ(bar_origin, origins[0]);
  ASSERT_EQ(foo_origin, origins[1]);

  // Remove Foo. Bar should remain.
  file_watcher.RemoveListener(foo_origin);
  origins = file_watcher.GetListeners();
  ASSERT_EQ(1U, origins.size());
  ASSERT_EQ(bar_origin, origins[0]);

  // Remove Bar. Nothing should remain.
  file_watcher.RemoveListener(bar_origin);
  origins = file_watcher.GetListeners();
  ASSERT_EQ(0U, origins.size());
}

TEST_F(FileManagerFileWatcherTest, AddSameExtensionMultipleTimes) {
  const base::FilePath kVirtualPath =
      base::FilePath::FromUTF8Unsafe("foo/bar.txt");
  const char kExtensionId[] = "extension-id";
  GURL source_url =
      extensions::Extension::GetBaseURLFromExtensionId(kExtensionId);
  url::Origin origin_1 =
      url::Origin::Create(source_url.DeprecatedGetOriginAsURL());
  url::Origin origin_2 = url::Origin::Create(source_url);

  FileWatcher file_watcher(kVirtualPath);
  // Add three times.
  file_watcher.AddListener(origin_1);
  file_watcher.AddListener(origin_2);
  file_watcher.AddListener(origin_1);

  std::vector<url::Origin> origins = file_watcher.GetListeners();
  ASSERT_EQ(1U, origins.size());
  ASSERT_EQ(origin_1, origins[0]);

  // Remove 1st time.
  file_watcher.RemoveListener(origin_2);
  origins = file_watcher.GetListeners();
  ASSERT_EQ(1U, origins.size());
  ASSERT_EQ(origin_2, origins[0]);

  // Remove 2nd time.
  file_watcher.RemoveListener(origin_2);
  origins = file_watcher.GetListeners();
  ASSERT_EQ(1U, origins.size());
  ASSERT_EQ(origin_1, origins[0]);

  // Remove 3rd time. The extension ID should be gone now.
  file_watcher.RemoveListener(origin_1);
  origins = file_watcher.GetListeners();
  ASSERT_EQ(0U, origins.size());
}

TEST_F(FileManagerFileWatcherTest, WatchLocalFile) {
  const base::FilePath kVirtualPath =
      base::FilePath::FromUTF8Unsafe("foo/bar.txt");
  const char kExtensionId[] = "extension-id";
  GURL listener_url =
      extensions::Extension::GetBaseURLFromExtensionId(kExtensionId);

  // Create a temporary directory.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Create a callback that will run when a change is detected.
  bool on_change_error = false;
  base::FilePath changed_path;
  base::RunLoop change_run_loop;
  base::FilePathWatcher::Callback change_callback =
      base::BindLambdaForTesting([&](const base::FilePath& path, bool error) {
        changed_path = path;
        on_change_error = error;
        change_run_loop.Quit();
      });

  // Create a callback that will run when the watcher is started.
  bool watcher_created = false;
  base::RunLoop start_run_loop;
  FileWatcher::BoolCallback start_callback =
      base::BindLambdaForTesting([&](bool success) {
        watcher_created = success;
        start_run_loop.Quit();
      });

  // Start watching changes in the temporary directory.
  FileWatcher file_watcher(kVirtualPath);
  file_watcher.AddListener(url::Origin::Create(listener_url));
  file_watcher.WatchLocalFile(profile(), temp_dir.GetPath(), change_callback,
                              std::move(start_callback));
  start_run_loop.Run();
  ASSERT_TRUE(watcher_created);

  // Create a temporary file in the temporary directory. The file watcher
  // should detect the change in the directory.
  base::FilePath temporary_file;
  ASSERT_TRUE(
      base::CreateTemporaryFileInDir(temp_dir.GetPath(), &temporary_file));

  // Wait until the directory change is notified. Also flush tasks in the
  // message loop since |change_callback| can be called multiple times.
  change_run_loop.Run();
  FlushMessageLoopTasks();

  ASSERT_FALSE(on_change_error);
  ASSERT_EQ(temp_dir.GetPath().value(), changed_path.value());
}

TEST_F(FileManagerFileWatcherTest, WatchCrostiniFile) {
  ash::FakeCiceroneClient* fake_cicerone_client =
      ash::FakeCiceroneClient::Get();

  const base::FilePath kVirtualPath("foo/bar.txt");
  const char kExtensionId[] = "extension-id";
  GURL listener_url =
      extensions::Extension::GetBaseURLFromExtensionId(kExtensionId);
  url::Origin origin = url::Origin::Create(listener_url);

  // Create a callback that will run when a change is detected.
  bool on_change_error = false;
  base::FilePath changed_path;
  base::RunLoop change_run_loop;
  base::FilePathWatcher::Callback change_callback =
      base::BindLambdaForTesting([&](const base::FilePath& path, bool error) {
        changed_path = path;
        on_change_error = error;
        change_run_loop.Quit();
      });

  // Create a callback that will run when the watcher is started.
  bool watcher_created = false;
  base::RunLoop start_run_loop;
  FileWatcher::BoolCallback start_callback =
      base::BindLambdaForTesting([&](bool success) {
        watcher_created = success;
        start_run_loop.Quit();
      });

  // Start watching changes in the crostini directory.
  base::FilePath crostini_dir = util::GetCrostiniMountDirectory(profile());
  FileWatcher file_watcher(kVirtualPath);
  file_watcher.AddListener(origin);
  file_watcher.WatchLocalFile(profile(), crostini_dir, change_callback,
                              std::move(start_callback));
  start_run_loop.Run();
  ASSERT_TRUE(watcher_created);

  // Send cicerone file changed signal.
  vm_tools::cicerone::FileWatchTriggeredSignal signal;
  signal.set_vm_name(crostini::kCrostiniDefaultVmName);
  signal.set_container_name(crostini::kCrostiniDefaultContainerName);
  signal.set_path("");
  fake_cicerone_client->NotifyFileWatchTriggered(signal);

  // Wait until the directory change is notified. Also flush tasks in the
  // message loop since |change_callback| can be called multiple times.
  change_run_loop.Run();
  FlushMessageLoopTasks();

  ASSERT_FALSE(on_change_error);
  ASSERT_EQ(crostini_dir.value(), changed_path.value());
}

}  // namespace
}  // namespace file_manager.
