// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/guest_os_file_watcher.h"

#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/guest_os/dbus_test_helper.h"
#include "chrome/browser/ash/guest_os/public/types.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/cicerone/fake_cicerone_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace guest_os {

class GuestOsFileWatcherTest : public testing::Test,
                               public guest_os::FakeVmServicesHelper {
 public:
  GuestOsFileWatcherTest() {
    watcher_callback_ = base::BindLambdaForTesting(
        [this](const base::FilePath& path, bool) { last_event_path_ = path; });
    signal_.set_owner_id(owner_id_);
    signal_.set_vm_name(guest_id_.vm_name);
    signal_.set_container_name(guest_id_.container_name);
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::string owner_id_{"owner-id"};
  GuestId guest_id_{VmType::UNKNOWN, "vm_name", ""};
  std::optional<base::FilePath> last_event_path_;
  vm_tools::cicerone::FileWatchTriggeredSignal signal_;
  base::FilePathWatcher::Callback watcher_callback_;
};

TEST_F(GuestOsFileWatcherTest, WatchNotTriggeredForWrongOwnerId) {
  GuestOsFileWatcher watcher("wrong-id", guest_id_, base::FilePath(),
                             base::FilePath());

  watcher.Watch(watcher_callback_, base::DoNothing());
  task_environment_.RunUntilIdle();
  FakeCiceroneClient()->NotifyFileWatchTriggered(signal_);

  EXPECT_EQ(last_event_path_, std::nullopt);
  EXPECT_EQ(FakeCiceroneClient()->add_file_watch_call_count(), 1);
}

TEST_F(GuestOsFileWatcherTest, WatchNotTriggeredForWrongContainerId) {
  GuestOsFileWatcher watcher(owner_id_,
                             GuestId(VmType::UNKNOWN, "DifferentVm", ""),
                             base::FilePath(), base::FilePath());

  watcher.Watch(watcher_callback_, base::DoNothing());
  task_environment_.RunUntilIdle();
  FakeCiceroneClient()->NotifyFileWatchTriggered(signal_);

  EXPECT_EQ(last_event_path_, std::nullopt);
  EXPECT_EQ(FakeCiceroneClient()->add_file_watch_call_count(), 1);
}

// Tests the full lifecycle - creating a watch, triggered with the right values,
// stops being triggered after getting destroyed.
TEST_F(GuestOsFileWatcherTest, WatchFullLifecycle) {
  auto watcher = std::make_unique<GuestOsFileWatcher>(
      owner_id_, guest_id_, base::FilePath("/mnt/fuse"),
      base::FilePath("a/path"));
  signal_.set_path("a/path");
  watcher->Watch(watcher_callback_, base::DoNothing());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(FakeCiceroneClient()->add_file_watch_call_count(), 1);

  FakeCiceroneClient()->NotifyFileWatchTriggered(signal_);
  EXPECT_EQ(last_event_path_, base::FilePath("/mnt/fuse/a/path"));

  // Should stop getting called after destroying the watcher.
  last_event_path_ = std::nullopt;
  watcher.reset();

  EXPECT_EQ(FakeCiceroneClient()->remove_file_watch_call_count(), 1);
  FakeCiceroneClient()->NotifyFileWatchTriggered(signal_);
  EXPECT_EQ(last_event_path_, std::nullopt);
}

}  // namespace guest_os
