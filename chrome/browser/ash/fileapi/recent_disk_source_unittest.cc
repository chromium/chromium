// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/fileapi/recent_disk_source.h"
#include "chrome/browser/ash/fileapi/recent_file.h"
#include "chrome/browser/ash/fileapi/recent_source.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/test_file_system_context.h"
#include "storage/common/file_system/file_system_mount_option.h"
#include "storage/common/file_system/file_system_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash {

class RecentDiskSourceTest : public testing::Test {
 public:
  RecentDiskSourceTest() : origin_("https://example.com/") {}

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    file_system_context_ = storage::CreateFileSystemContextForTesting(
        /*quota_manager_proxy=*/nullptr, temp_dir_.GetPath());

    mount_point_name_ =
        file_manager::util::GetDownloadsMountPointName(profile_.get());

    ASSERT_TRUE(
        storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
            mount_point_name_, storage::kFileSystemTypeTest,
            storage::FileSystemMountOption(), base::FilePath()));

    source_ = std::make_unique<RecentDiskSource>(
        mount_point_name_, false /* ignore_dotfiles */, 0 /* max_depth */,
        uma_histogram_name_);
  }

  void TearDown() override {
    storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
        mount_point_name_);
  }

 protected:
  bool CreateEmptyFile(const std::string& filename, const base::Time& time) {
    base::File file(temp_dir_.GetPath().Append(filename),
                    base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    if (!file.IsValid())
      return false;

    return file.SetTimes(time, time);
  }

  std::vector<RecentFile> GetRecentFiles(
      size_t max_files,
      const base::Time& cutoff_time,
      RecentSource::FileType file_type = RecentSource::FileType::kAll) {
    std::vector<RecentFile> files;

    base::RunLoop run_loop;

    source_->GetRecentFiles(RecentSource::Params(
        file_system_context_.get(), origin_, max_files, cutoff_time, file_type,
        base::BindOnce(
            [](base::RunLoop* run_loop, std::vector<RecentFile>* out_files,
               std::vector<RecentFile> files) {
              run_loop->Quit();
              *out_files = std::move(files);
            },
            &run_loop, &files)));

    run_loop.Run();

    return files;
  }

  content::BrowserTaskEnvironment task_environment_;
  const GURL origin_;
  std::unique_ptr<TestingProfile> profile_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  std::string mount_point_name_;
  const std::string uma_histogram_name_ = "uma_histogram_name";
  std::unique_ptr<RecentDiskSource> source_;
  base::Time base_time_;
};

TEST_F(RecentDiskSourceTest, GetRecentFiles) {
  // Oldest
  ASSERT_TRUE(CreateEmptyFile("1.jpg", base::Time::FromJavaTime(1000)));
  ASSERT_TRUE(CreateEmptyFile("2.jpg", base::Time::FromJavaTime(2000)));
  ASSERT_TRUE(CreateEmptyFile("3.jpg", base::Time::FromJavaTime(3000)));
  ASSERT_TRUE(CreateEmptyFile("4.jpg", base::Time::FromJavaTime(4000)));
  // Newest

  std::vector<RecentFile> files = GetRecentFiles(3, base::Time());

  std::sort(files.begin(), files.end(), RecentFileComparator());

  ASSERT_EQ(3u, files.size());
  EXPECT_EQ("4.jpg", files[0].url().path().BaseName().value());
  EXPECT_EQ(base::Time::FromJavaTime(4000), files[0].last_modified());
  EXPECT_EQ("3.jpg", files[1].url().path().BaseName().value());
  EXPECT_EQ(base::Time::FromJavaTime(3000), files[1].last_modified());
  EXPECT_EQ("2.jpg", files[2].url().path().BaseName().value());
  EXPECT_EQ(base::Time::FromJavaTime(2000), files[2].last_modified());
}

TEST_F(RecentDiskSourceTest, GetRecentFiles_CutoffTime) {
  // Oldest
  ASSERT_TRUE(CreateEmptyFile("1.jpg", base::Time::FromJavaTime(1000)));
  ASSERT_TRUE(CreateEmptyFile("2.jpg", base::Time::FromJavaTime(2000)));
  ASSERT_TRUE(CreateEmptyFile("3.jpg", base::Time::FromJavaTime(3000)));
  ASSERT_TRUE(CreateEmptyFile("4.jpg", base::Time::FromJavaTime(4000)));
  // Newest

  std::vector<RecentFile> files =
      GetRecentFiles(3, base::Time::FromJavaTime(2500));

  std::sort(files.begin(), files.end(), RecentFileComparator());

  ASSERT_EQ(2u, files.size());
  EXPECT_EQ("4.jpg", files[0].url().path().BaseName().value());
  EXPECT_EQ(base::Time::FromJavaTime(4000), files[0].last_modified());
  EXPECT_EQ("3.jpg", files[1].url().path().BaseName().value());
  EXPECT_EQ(base::Time::FromJavaTime(3000), files[1].last_modified());
}

TEST_F(RecentDiskSourceTest, IgnoreDotFiles) {
  ASSERT_TRUE(base::CreateDirectory(temp_dir_.GetPath().Append(".ignore")));
  ASSERT_TRUE(base::CreateDirectory(temp_dir_.GetPath().Append("noignore")));

  // Oldest
  ASSERT_TRUE(
      CreateEmptyFile("noignore/1.jpg", base::Time::FromJavaTime(1000)));
  ASSERT_TRUE(CreateEmptyFile(".ignore/2.jpg", base::Time::FromJavaTime(2000)));
  ASSERT_TRUE(CreateEmptyFile("3.jpg", base::Time::FromJavaTime(3000)));
  ASSERT_TRUE(CreateEmptyFile(".4.jpg", base::Time::FromJavaTime(4000)));
  // Newest

  std::vector<RecentFile> files = GetRecentFiles(4, base::Time());

  std::sort(files.begin(), files.end(), RecentFileComparator());

  ASSERT_EQ(4u, files.size());
  EXPECT_EQ(".4.jpg", files[0].url().path().BaseName().value());
  EXPECT_EQ(base::Time::FromJavaTime(4000), files[0].last_modified());
  EXPECT_EQ("3.jpg", files[1].url().path().BaseName().value());
  EXPECT_EQ(base::Time::FromJavaTime(3000), files[1].last_modified());
  EXPECT_EQ("2.jpg", files[2].url().path().BaseName().value());
  EXPECT_EQ(base::Time::FromJavaTime(2000), files[2].last_modified());
  EXPECT_EQ("1.jpg", files[3].url().path().BaseName().value());
  EXPECT_EQ(base::Time::FromJavaTime(1000), files[3].last_modified());

  source_ = std::make_unique<RecentDiskSource>(
      mount_point_name_, true /* ignore_dotfiles */, 0 /* max_depth */,
      uma_histogram_name_);

  files = GetRecentFiles(4, base::Time());

  std::sort(files.begin(), files.end(), RecentFileComparator());

  ASSERT_EQ(2u, files.size());
  EXPECT_EQ("3.jpg", files[0].url().path().BaseName().value());
  EXPECT_EQ(base::Time::FromJavaTime(3000), files[0].last_modified());
  EXPECT_EQ("1.jpg", files[1].url().path().BaseName().value());
  EXPECT_EQ(base::Time::FromJavaTime(1000), files[1].last_modified());
}

TEST_F(RecentDiskSourceTest, MaxDepth) {
  ASSERT_TRUE(base::CreateDirectory(temp_dir_.GetPath().Append("a")));
  ASSERT_TRUE(base::CreateDirectory(temp_dir_.GetPath().Append("a/b")));
  ASSERT_TRUE(base::CreateDirectory(temp_dir_.GetPath().Append("a/b/c")));

  // Oldest
  ASSERT_TRUE(CreateEmptyFile("1.jpg", base::Time::FromJavaTime(1000)));
  ASSERT_TRUE(CreateEmptyFile("a/2.jpg", base::Time::FromJavaTime(2000)));
  ASSERT_TRUE(CreateEmptyFile("a/b/3.jpg", base::Time::FromJavaTime(3000)));
  ASSERT_TRUE(CreateEmptyFile("a/b/c/4.jpg", base::Time::FromJavaTime(4000)));
  // Newest

  std::vector<RecentFile> files = GetRecentFiles(4, base::Time());
  ASSERT_EQ(4u, files.size());

  source_ = std::make_unique<RecentDiskSource>(mount_point_name_, false, 2,
                                               uma_histogram_name_);

  files = GetRecentFiles(4, base::Time());

  std::sort(files.begin(), files.end(), RecentFileComparator());

  ASSERT_EQ(2u, files.size());
  EXPECT_EQ("2.jpg", files[0].url().path().BaseName().value());
  EXPECT_EQ(base::Time::FromJavaTime(2000), files[0].last_modified());
  EXPECT_EQ("1.jpg", files[1].url().path().BaseName().value());
  EXPECT_EQ(base::Time::FromJavaTime(1000), files[1].last_modified());
}

TEST_F(RecentDiskSourceTest, GetAudioFiles) {
  // Oldest
  ASSERT_TRUE(CreateEmptyFile("1.jpg", base::Time::FromJavaTime(1000)));
  ASSERT_TRUE(CreateEmptyFile("2.mp4", base::Time::FromJavaTime(2000)));
  ASSERT_TRUE(CreateEmptyFile("3.png", base::Time::FromJavaTime(3000)));
  ASSERT_TRUE(CreateEmptyFile("4.mp3", base::Time::FromJavaTime(4000)));
  ASSERT_TRUE(CreateEmptyFile("5.gif", base::Time::FromJavaTime(5000)));
  ASSERT_TRUE(CreateEmptyFile("6.webm", base::Time::FromJavaTime(6000)));
  ASSERT_TRUE(CreateEmptyFile("7.amr", base::Time::FromJavaTime(7000)));
  // Newest

  std::vector<RecentFile> files =
      GetRecentFiles(7, base::Time(), RecentSource::FileType::kAudio);

  std::sort(files.begin(), files.end(), RecentFileComparator());

  ASSERT_EQ(2u, files.size());
  EXPECT_EQ("7.amr", files[0].url().path().BaseName().value());
  EXPECT_EQ(base::Time::FromJavaTime(7000), files[0].last_modified());
  EXPECT_EQ("4.mp3", files[1].url().path().BaseName().value());
  EXPECT_EQ(base::Time::FromJavaTime(4000), files[1].last_modified());
}

TEST_F(RecentDiskSourceTest, GetImageFiles) {
  // Oldest
  ASSERT_TRUE(CreateEmptyFile("1.jpg", base::Time::FromJavaTime(1000)));
  ASSERT_TRUE(CreateEmptyFile("2.mp4", base::Time::FromJavaTime(2000)));
  ASSERT_TRUE(CreateEmptyFile("3.png", base::Time::FromJavaTime(3000)));
  ASSERT_TRUE(CreateEmptyFile("4.mp3", base::Time::FromJavaTime(4000)));
  ASSERT_TRUE(CreateEmptyFile("5.gif", base::Time::FromJavaTime(5000)));
  ASSERT_TRUE(CreateEmptyFile("6.webm", base::Time::FromJavaTime(6000)));
  // RAW images are supported
  ASSERT_TRUE(CreateEmptyFile("7.dng", base::Time::FromJavaTime(7000)));
  ASSERT_TRUE(CreateEmptyFile("8.nef", base::Time::FromJavaTime(8000)));
  // Newest

  std::vector<RecentFile> files =
      GetRecentFiles(8, base::Time(), RecentSource::FileType::kImage);

  std::sort(files.begin(), files.end(), RecentFileComparator());

  ASSERT_EQ(5u, files.size());
  EXPECT_EQ("8.nef", files[0].url().path().BaseName().value());
  EXPECT_EQ(base::Time::FromJavaTime(8000), files[0].last_modified());
  EXPECT_EQ("7.dng", files[1].url().path().BaseName().value());
  EXPECT_EQ(base::Time::FromJavaTime(7000), files[1].last_modified());
  EXPECT_EQ("5.gif", files[2].url().path().BaseName().value());
  EXPECT_EQ(base::Time::FromJavaTime(5000), files[2].last_modified());
  EXPECT_EQ("3.png", files[3].url().path().BaseName().value());
  EXPECT_EQ(base::Time::FromJavaTime(3000), files[3].last_modified());
  EXPECT_EQ("1.jpg", files[4].url().path().BaseName().value());
  EXPECT_EQ(base::Time::FromJavaTime(1000), files[4].last_modified());
}

TEST_F(RecentDiskSourceTest, GetVideoFiles) {
  // Oldest
  ASSERT_TRUE(CreateEmptyFile("1.jpg", base::Time::FromJavaTime(1000)));
  ASSERT_TRUE(CreateEmptyFile("2.mp4", base::Time::FromJavaTime(2000)));
  ASSERT_TRUE(CreateEmptyFile("3.png", base::Time::FromJavaTime(3000)));
  ASSERT_TRUE(CreateEmptyFile("4.mp3", base::Time::FromJavaTime(4000)));
  ASSERT_TRUE(CreateEmptyFile("5.gif", base::Time::FromJavaTime(5000)));
  ASSERT_TRUE(CreateEmptyFile("6.webm", base::Time::FromJavaTime(6000)));
  ASSERT_TRUE(CreateEmptyFile("7.avi", base::Time::FromJavaTime(7000)));
  ASSERT_TRUE(CreateEmptyFile("8.mov", base::Time::FromJavaTime(8000)));
  // *.wmv is not supported yet.
  ASSERT_TRUE(CreateEmptyFile("9.wmv", base::Time::FromJavaTime(9000)));
  // Newest

  std::vector<RecentFile> files =
      GetRecentFiles(9, base::Time(), RecentSource::FileType::kVideo);

  std::sort(files.begin(), files.end(), RecentFileComparator());

  ASSERT_EQ(4u, files.size());
  EXPECT_EQ("8.mov", files[0].url().path().BaseName().value());
  EXPECT_EQ(base::Time::FromJavaTime(8000), files[0].last_modified());
  EXPECT_EQ("7.avi", files[1].url().path().BaseName().value());
  EXPECT_EQ(base::Time::FromJavaTime(7000), files[1].last_modified());
  EXPECT_EQ("6.webm", files[2].url().path().BaseName().value());
  EXPECT_EQ(base::Time::FromJavaTime(6000), files[2].last_modified());
  EXPECT_EQ("2.mp4", files[3].url().path().BaseName().value());
  EXPECT_EQ(base::Time::FromJavaTime(2000), files[3].last_modified());
}

TEST_F(RecentDiskSourceTest, GetDocumentFiles) {
  // Oldest
  ASSERT_TRUE(CreateEmptyFile("1.jpg", base::Time::FromJavaTime(1000)));
  ASSERT_TRUE(CreateEmptyFile("2.mp4", base::Time::FromJavaTime(2000)));
  ASSERT_TRUE(CreateEmptyFile("3.png", base::Time::FromJavaTime(3000)));
  ASSERT_TRUE(CreateEmptyFile("4.doc", base::Time::FromJavaTime(4000)));
  ASSERT_TRUE(CreateEmptyFile("5.gif", base::Time::FromJavaTime(5000)));
  ASSERT_TRUE(CreateEmptyFile("6.txt", base::Time::FromJavaTime(6000)));
  ASSERT_TRUE(CreateEmptyFile("7.avi", base::Time::FromJavaTime(7000)));
  ASSERT_TRUE(CreateEmptyFile("8.gdoc", base::Time::FromJavaTime(8000)));
  // Newest

  std::vector<RecentFile> files =
      GetRecentFiles(8, base::Time(), RecentSource::FileType::kDocument);

  std::sort(files.begin(), files.end(), RecentFileComparator());

  ASSERT_EQ(3u, files.size());
  EXPECT_EQ("8.gdoc", files[0].url().path().BaseName().value());
  EXPECT_EQ(base::Time::FromJavaTime(8000), files[0].last_modified());
  EXPECT_EQ("6.txt", files[1].url().path().BaseName().value());
  EXPECT_EQ(base::Time::FromJavaTime(6000), files[1].last_modified());
  EXPECT_EQ("4.doc", files[2].url().path().BaseName().value());
  EXPECT_EQ(base::Time::FromJavaTime(4000), files[2].last_modified());
}

TEST_F(RecentDiskSourceTest, GetRecentFiles_UmaStats) {
  base::HistogramTester histogram_tester;

  GetRecentFiles(3, base::Time());

  histogram_tester.ExpectTotalCount(uma_histogram_name_, 1);
}

}  // namespace ash
