// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/android/offline_page_archive_publisher_impl.h"

#include "base/android/build_info.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_simple_task_runner.h"
#include "components/offline_pages/core/archive_manager.h"
#include "components/offline_pages/core/model/offline_page_item_generator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const int64_t kDownloadId = 42LL;
}  // namespace

namespace offline_pages {

class OfflinePageArchivePublisherImplTest : public testing::Test {
 public:
  OfflinePageArchivePublisherImplTest()
      : task_runner_(new base::TestSimpleTaskRunner),
        task_runner_current_default_handle_(task_runner_) {}
  ~OfflinePageArchivePublisherImplTest() override {}

  void SetUp() override;
  void PumpLoop();

  OfflinePageItemGenerator* page_generator() { return &page_generator_; }

  const base::FilePath& temporary_dir_path() {
    return temporary_dir_.GetPath();
  }
  const base::FilePath& private_archive_dir_path() {
    return private_archive_dir_.GetPath();
  }
  const base::FilePath& public_archive_dir_path() {
    return public_archive_dir_.GetPath();
  }
  const PublishArchiveResult& publish_archive_result() {
    return publish_archive_result_;
  }
  scoped_refptr<base::SequencedTaskRunner> task_runner() {
    return task_runner_;
  }
  base::WeakPtr<OfflinePageArchivePublisherImplTest> get_weak_ptr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void PublishArchiveDone(const OfflinePageItem& offline_page,
                          PublishArchiveResult archive_result);

 private:
  base::ScopedTempDir temporary_dir_;
  base::ScopedTempDir private_archive_dir_;
  base::ScopedTempDir public_archive_dir_;
  OfflinePageItemGenerator page_generator_;
  PublishArchiveResult publish_archive_result_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::SingleThreadTaskRunner::CurrentDefaultHandle
      task_runner_current_default_handle_;
  base::WeakPtrFactory<OfflinePageArchivePublisherImplTest> weak_ptr_factory_{
      this};
};

void OfflinePageArchivePublisherImplTest::SetUp() {
  ASSERT_TRUE(temporary_dir_.CreateUniqueTempDir());
  ASSERT_TRUE(private_archive_dir_.CreateUniqueTempDir());
  ASSERT_TRUE(public_archive_dir_.CreateUniqueTempDir());
}

class TestArchivePublisherDelegate
    : public OfflinePageArchivePublisherImpl::Delegate {
 public:
  TestArchivePublisherDelegate(int64_t id_to_use, bool installed)
      : download_id_(id_to_use), last_removed_id_(0), installed_(installed) {}

  bool IsDownloadManagerInstalled() override { return installed_; }
  PublishArchiveResult AddCompletedDownload(
      const OfflinePageItem& page) override {
    return {SavePageResult::SUCCESS, {download_id_, page.file_path}};
  }

  int Remove(
      const std::vector<int64_t>& android_download_manager_ids) override {
    int count = static_cast<int>(android_download_manager_ids.size());
    if (count > 0)
      last_removed_id_ = android_download_manager_ids[count - 1];
    return count;
  }

  int64_t last_removed_id() const { return last_removed_id_; }

 private:
  int64_t download_id_;
  int64_t last_removed_id_;
  bool installed_;
};

void OfflinePageArchivePublisherImplTest::PublishArchiveDone(
    const OfflinePageItem& offline_page,
    PublishArchiveResult archive_result) {
  publish_archive_result_ = archive_result;
}

void OfflinePageArchivePublisherImplTest::PumpLoop() {
  task_runner_->RunUntilIdle();
}

TEST_F(OfflinePageArchivePublisherImplTest, PublishArchive) {
  ArchiveManager archive_manager(temporary_dir_path(),
                                 private_archive_dir_path(),
                                 public_archive_dir_path(), task_runner());

  OfflinePageArchivePublisherImpl publisher(&archive_manager);
  TestArchivePublisherDelegate delegate(kDownloadId, true);
  publisher.SetDelegateForTesting(&delegate);

  // Put an offline page into the private dir, adjust the FilePath.
  page_generator()->SetArchiveDirectory(temporary_dir_path());
  OfflinePageItem offline_page = page_generator()->CreateItemWithTempFile();
  base::FilePath old_file_path = offline_page.file_path;
  base::FilePath new_file_path =
      public_archive_dir_path().Append(offline_page.file_path.BaseName());

  publisher.PublishArchive(
      offline_page, base::SingleThreadTaskRunner::GetCurrentDefault(),
      base::BindOnce(&OfflinePageArchivePublisherImplTest::PublishArchiveDone,
                     get_weak_ptr()));
  PumpLoop();

  EXPECT_EQ(SavePageResult::SUCCESS, publish_archive_result().move_result);
  EXPECT_EQ(kDownloadId, publish_archive_result().id.download_id);

  // The file move should not happen on Android Q and later.
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SDK_VERSION_Q) {
    // Check there is a file in the new location.
    EXPECT_TRUE(public_archive_dir_path().IsParent(
        publish_archive_result().id.new_file_path));
    EXPECT_TRUE(base::PathExists(publish_archive_result().id.new_file_path));
    // Check there is no longer a file in the old location.
    EXPECT_FALSE(base::PathExists(old_file_path));
  } else {
    EXPECT_FALSE(public_archive_dir_path().IsParent(
        publish_archive_result().id.new_file_path));
    // new_file_path should be the same as the page's file path.
    EXPECT_TRUE(base::PathExists(publish_archive_result().id.new_file_path));
    EXPECT_TRUE(base::PathExists(old_file_path));
  }
}

TEST_F(OfflinePageArchivePublisherImplTest, UnpublishArchives) {
  ArchiveManager archive_manager(temporary_dir_path(),
                                 private_archive_dir_path(),
                                 public_archive_dir_path(), task_runner());

  TestArchivePublisherDelegate delegate(kDownloadId, true);
  OfflinePageArchivePublisherImpl publisher(&archive_manager);
  publisher.SetDelegateForTesting(&delegate);

  // This needs to be very close to a real content URI or DeleteContentUri will
  // throw an exception.
  base::FilePath test_content_uri =
      base::FilePath("content://downloads/download/43");

  std::vector<PublishedArchiveId> ids_to_remove{
      {kDownloadId, base::FilePath()},
      {kArchivePublishedWithoutDownloadId, test_content_uri}};
  publisher.UnpublishArchives(std::move(ids_to_remove));

  EXPECT_EQ(kDownloadId, delegate.last_removed_id());
}

// TODO(petewil): Add test cases for move failed, and adding to ADM failed.

}  // namespace offline_pages
