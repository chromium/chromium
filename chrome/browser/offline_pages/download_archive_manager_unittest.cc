// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/download_archive_manager.h"

#include "base/macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/common/pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {
const char* kPrivateDir = "/private/";
const char* kTemporaryDir = "/temporary/";
const char* kPublicDir = "/public/";
const char* kChromePublicSdCardDir =
    "/sd-card/1234-5678/Android/data/org.chromium.chrome/files/Download";
}  // namespace

class DownloadArchiveManagerTest : public testing::Test {
 public:
  DownloadArchiveManagerTest() = default;
  ~DownloadArchiveManagerTest() override = default;

  void SetUp() override;
  void TearDown() override;
  void PumpLoop();

  sync_preferences::TestingPrefServiceSyncable* prefs() { return &prefs_; }
  DownloadArchiveManager* archive_manager() { return archive_manager_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  sync_preferences::TestingPrefServiceSyncable prefs_;
  std::unique_ptr<DownloadArchiveManager> archive_manager_;
  DISALLOW_COPY_AND_ASSIGN(DownloadArchiveManagerTest);
};

void DownloadArchiveManagerTest::SetUp() {
  // Set up preferences to point to kChromePublicSdCardDir.
  DownloadPrefs::RegisterProfilePrefs(prefs()->registry());
  prefs()->SetString(prefs::kDownloadDefaultDirectory, kChromePublicSdCardDir);

  // Create a DownloadArchiveManager to use.
  archive_manager_.reset(new DownloadArchiveManager(
      base::FilePath(kTemporaryDir), base::FilePath(kPrivateDir),
      base::FilePath(kPublicDir), base::ThreadTaskRunnerHandle::Get(),
      prefs()));
}

void DownloadArchiveManagerTest::TearDown() {
  archive_manager_.release();
}

TEST_F(DownloadArchiveManagerTest, UseDownloadDirFromPreferences) {
  base::FilePath download_dir = archive_manager()->GetPublicArchivesDir();
  ASSERT_EQ(kChromePublicSdCardDir, download_dir.AsUTF8Unsafe());
}

TEST_F(DownloadArchiveManagerTest, NullPrefs) {
  DownloadArchiveManager download_archive_manager(
      base::FilePath(kTemporaryDir), base::FilePath(kPrivateDir),
      base::FilePath(kPublicDir), base::ThreadTaskRunnerHandle::Get(), nullptr);

  base::FilePath download_dir = download_archive_manager.GetPublicArchivesDir();
  ASSERT_EQ(kPublicDir, download_dir.AsUTF8Unsafe());
}

}  // namespace offline_pages
