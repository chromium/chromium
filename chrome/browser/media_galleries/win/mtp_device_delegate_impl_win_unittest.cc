// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/media_galleries/media_file_system_registry.h"
#include "chrome/browser/media_galleries/media_galleries_test_util.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/storage_monitor/storage_info.h"
#include "components/storage_monitor/storage_monitor.h"
#include "components/storage_monitor/test_portable_device_watcher_win.h"
#include "components/storage_monitor/test_storage_monitor.h"
#include "components/storage_monitor/test_storage_monitor_win.h"
#include "components/storage_monitor/test_volume_mount_watcher_win.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_system.h"
#include "testing/gtest/include/gtest/gtest.h"

using storage_monitor::PortableDeviceWatcherWin;
using storage_monitor::StorageInfo;
using storage_monitor::StorageMonitor;
using storage_monitor::TestPortableDeviceWatcherWin;
using storage_monitor::TestStorageMonitor;
using storage_monitor::TestStorageMonitorWin;
using storage_monitor::TestVolumeMountWatcherWin;

namespace {

typedef std::map<MediaGalleryPrefId, MediaFileSystemInfo> FSInfoMap;

void GetGalleryInfoCallback(
    FSInfoMap* results,
    const std::vector<MediaFileSystemInfo>& file_systems) {
  for (size_t i = 0; i < file_systems.size(); ++i) {
    ASSERT_FALSE(base::Contains(*results, file_systems[i].pref_id));
    (*results)[file_systems[i].pref_id] = file_systems[i];
  }
}

}  // namespace

class MTPDeviceDelegateImplWinTest : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override;
  void TearDown() override;

  void ProcessAttach(const std::string& id,
                     const base::string16& name,
                     const base::FilePath::StringType& location);
  std::string AttachDevice(StorageInfo::Type type,
                           const std::string& unique_id,
                           const base::FilePath& location);
  void CheckGalleryInfo(const MediaFileSystemInfo& info,
                        const base::string16& name,
                        const base::FilePath& path,
                        bool removable,
                        bool media_device);

  // Pointer to the storage monitor. Owned by TestingBrowserProcess.
  TestStorageMonitorWin* monitor_;
  scoped_refptr<extensions::Extension> extension_;

  EnsureMediaDirectoriesExists media_directories_;
};

void MTPDeviceDelegateImplWinTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();

  TestStorageMonitor::Destroy();
  TestPortableDeviceWatcherWin* portable_device_watcher =
      new TestPortableDeviceWatcherWin;
  TestVolumeMountWatcherWin* mount_watcher = new TestVolumeMountWatcherWin;
  portable_device_watcher->set_use_dummy_mtp_storage_info(true);
  std::unique_ptr<TestStorageMonitorWin> monitor(
      new TestStorageMonitorWin(mount_watcher, portable_device_watcher));
  TestingBrowserProcess* browser_process = TestingBrowserProcess::GetGlobal();
  DCHECK(browser_process);
  monitor_ = monitor.get();
  StorageMonitor::SetStorageMonitorForTesting(std::move(monitor));

  base::RunLoop runloop;
  browser_process->media_file_system_registry()->GetPreferences(profile())->
      EnsureInitialized(runloop.QuitClosure());
  runloop.Run();

  extensions::TestExtensionSystem* extension_system(
      static_cast<extensions::TestExtensionSystem*>(
          extensions::ExtensionSystem::Get(profile())));
  extension_system->CreateExtensionService(
      base::CommandLine::ForCurrentProcess(), base::FilePath(), false);

  std::vector<std::string> all_permissions;
  all_permissions.push_back("allAutoDetected");
  all_permissions.push_back("read");
  extension_ = AddMediaGalleriesApp("all", all_permissions, profile());
}

void MTPDeviceDelegateImplWinTest::TearDown() {
  ChromeRenderViewHostTestHarness::TearDown();

  TestingBrowserProcess::DeleteInstance();

  // Windows storage monitor must be destroyed after the MediaFileSystemRegistry
  // owned by TestingBrowserProcess because it uses it in its destructor.
  TestStorageMonitor::Destroy();
}

void MTPDeviceDelegateImplWinTest::ProcessAttach(
    const std::string& id,
    const base::string16& label,
    const base::FilePath::StringType& location) {
  StorageInfo info(id, location, label, base::string16(), base::string16(), 0);
  monitor_->receiver()->ProcessAttach(info);
}

std::string MTPDeviceDelegateImplWinTest::AttachDevice(
    StorageInfo::Type type,
    const std::string& unique_id,
    const base::FilePath& location) {
  std::string device_id = StorageInfo::MakeDeviceId(type, unique_id);
  DCHECK(StorageInfo::IsRemovableDevice(device_id));
  base::string16 label = location.LossyDisplayName();
  ProcessAttach(device_id, label, location.value());
  base::RunLoop().RunUntilIdle();
  return device_id;
}

void MTPDeviceDelegateImplWinTest::CheckGalleryInfo(
    const MediaFileSystemInfo& info,
    const base::string16& name,
    const base::FilePath& path,
    bool removable,
    bool media_device) {
  EXPECT_EQ(name, info.name);
  EXPECT_EQ(path, info.path);
  EXPECT_EQ(removable, info.removable);
  EXPECT_EQ(media_device, info.media_device);
  EXPECT_NE(0UL, info.pref_id);

  if (removable)
    EXPECT_NE(0UL, info.transient_device_id.size());
  else
    EXPECT_EQ(0UL, info.transient_device_id.size());
}

// TODO(https://crbug.com/868254): Failing on Win7 Tests (1). Fix and enable.
TEST_F(MTPDeviceDelegateImplWinTest, DISABLED_GalleryNameMTP) {
  base::FilePath location(
      PortableDeviceWatcherWin::GetStoragePathFromStorageId(
          TestPortableDeviceWatcherWin::kStorageUniqueIdA));
  AttachDevice(StorageInfo::MTP_OR_PTP, "mtp_fake_id", location);

  FSInfoMap results;
  MediaFileSystemRegistry* registry =
      g_browser_process->media_file_system_registry();
  registry->GetMediaFileSystemsForExtension(
      web_contents(), extension_.get(),
      base::Bind(&GetGalleryInfoCallback, base::Unretained(&results)));
  content::RunAllTasksUntilIdle();

  ASSERT_EQ(media_directories_.num_galleries() + 1u, results.size());
  bool checked = false;
  for (FSInfoMap::iterator i = results.begin(); i != results.end(); ++i) {
    MediaFileSystemInfo info = i->second;
    if (info.path == location) {
      CheckGalleryInfo(info, location.LossyDisplayName(), location, true, true);
      checked = true;
      break;
    }
  }
  EXPECT_TRUE(checked);
}
