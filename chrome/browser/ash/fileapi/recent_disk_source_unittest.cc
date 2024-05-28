// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
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

namespace {

// Allows one to specify just the parameters that are relevant to a given test.
struct TestParams {
  TestParams& Query(const std::string query) {
    query_ = query;
    return *this;
  }

  TestParams& CutoffTime(const base::Time& cutoff_time) {
    cutoff_time_ = cutoff_time;
    return *this;
  }

  TestParams& FileType(const RecentSource::FileType type) {
    file_type_ = type;
    return *this;
  }

  TestParams& IgnoreDotFiles(bool ignore_dot_files) {
    ignore_dot_files_ = ignore_dot_files;
    return *this;
  }

  TestParams& MaxDepth(int max_depth) {
    max_depth_ = max_depth;
    return *this;
  }

  TestParams& MaxFiles(int max_files) {
    max_files_ = max_files;
    return *this;
  }

  RecentSource::Params MakeParams(storage::FileSystemContext* context,
                                  const int32_t call_id,
                                  const GURL& origin) {
    return RecentSource::Params(context, call_id, origin, query_, max_files_,
                                cutoff_time_, base::TimeTicks::Max(),
                                file_type_);
  }

  std::unique_ptr<RecentDiskSource> MakeSource(
      const std::string& mount_point_name,
      const std::string& uma_histogram_name) {
    return std::make_unique<RecentDiskSource>(
        extensions::api::file_manager_private::VolumeType::kTesting,
        mount_point_name, ignore_dot_files_, max_depth_, uma_histogram_name);
  }

  std::string query_ = "";
  base::Time cutoff_time_ = base::Time::Min();
  RecentSource::FileType file_type_ = RecentSource::FileType::kAll;
  bool ignore_dot_files_ = false;
  int max_depth_ = 0;
  size_t max_files_ = 100;
};

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
  }

  void TearDown() override {
    storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
        mount_point_name_);
  }

 protected:
  bool CreateEmptyFile(const std::string& filename, const base::Time& time) {
    base::File file(temp_dir_.GetPath().Append(filename),
                    base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    if (!file.IsValid()) {
      return false;
    }

    return file.SetTimes(time, time);
  }

  std::vector<RecentFile> GetRecentFiles(TestParams params) {
    std::vector<RecentFile> files;
    base::RunLoop run_loop;

    auto source = params.MakeSource(mount_point_name_, uma_histogram_name_);
    source->GetRecentFiles(
        params.MakeParams(file_system_context_.get(), 0, origin_),
        base::BindOnce(
            [](base::RunLoop* run_loop, std::vector<RecentFile>* out_files,
               std::vector<RecentFile> files) {
              run_loop->Quit();
              *out_files = std::move(files);
            },
            &run_loop, &files));

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
  base::Time base_time_;
};

TEST_F(RecentDiskSourceTest, GetRecentFiles) {
  // Oldest
  ASSERT_TRUE(
      CreateEmptyFile("1.jpg", base::Time::FromSecondsSinceUnixEpoch(1)));
  ASSERT_TRUE(
      CreateEmptyFile("2.jpg", base::Time::FromSecondsSinceUnixEpoch(2)));
  ASSERT_TRUE(
      CreateEmptyFile("3.jpg", base::Time::FromSecondsSinceUnixEpoch(3)));
  ASSERT_TRUE(
      CreateEmptyFile("4.jpg", base::Time::FromSecondsSinceUnixEpoch(4)));
  // Newest

  std::vector<RecentFile> files = GetRecentFiles(TestParams().MaxFiles(3));

  ASSERT_EQ(3u, files.size());
  EXPECT_EQ("4.jpg", files[0].url().path().BaseName().value());
  EXPECT_EQ(base::Time::FromSecondsSinceUnixEpoch(4), files[0].last_modified());
  EXPECT_EQ("3.jpg", files[1].url().path().BaseName().value());
  EXPECT_EQ(base::Time::FromSecondsSinceUnixEpoch(3), files[1].last_modified());
  EXPECT_EQ("2.jpg", files[2].url().path().BaseName().value());
  EXPECT_EQ(base::Time::FromSecondsSinceUnixEpoch(2), files[2].last_modified());

  files = GetRecentFiles(TestParams().Query("4").MaxFiles(3));
  ASSERT_EQ(1u, files.size());
  EXPECT_EQ("4.jpg", files[0].url().path().BaseName().value());

  files = GetRecentFiles(TestParams().Query("foo").MaxFiles(3));
  ASSERT_EQ(0u, files.size());
}

TEST_F(RecentDiskSourceTest, GetRecentFiles_CutoffTime) {
  // Oldest
  ASSERT_TRUE(
      CreateEmptyFile("1.jpg", base::Time::FromSecondsSinceUnixEpoch(1)));
  ASSERT_TRUE(
      CreateEmptyFile("2.jpg", base::Time::FromSecondsSinceUnixEpoch(2)));
  ASSERT_TRUE(
      CreateEmptyFile("3.jpg", base::Time::FromSecondsSinceUnixEpoch(3)));
  ASSERT_TRUE(
      CreateEmptyFile("4.jpg", base::Time::FromSecondsSinceUnixEpoch(4)));
  // Newest

  std::vector<RecentFile> files = GetRecentFiles(
      TestParams()
          .CutoffTime(base::Time::FromMillisecondsSinceUnixEpoch(2500))
          .MaxFiles(3));

  ASSERT_EQ(2u, files.size());
  EXPECT_EQ("4.jpg", files[0].url().path().BaseName().value());
  EXPECT_EQ(base::Time::FromSecondsSinceUnixEpoch(4), files[0].last_modified());
  EXPECT_EQ("3.jpg", files[1].url().path().BaseName().value());
  EXPECT_EQ(base::Time::FromSecondsSinceUnixEpoch(3), files[1].last_modified());
}

TEST_F(RecentDiskSourceTest, IgnoreDotFiles) {
  ASSERT_TRUE(base::CreateDirectory(temp_dir_.GetPath().Append(".ignore")));
  ASSERT_TRUE(base::CreateDirectory(temp_dir_.GetPath().Append("noignore")));

  // Oldest
  ASSERT_TRUE(CreateEmptyFile("noignore/1.jpg",
                              base::Time::FromSecondsSinceUnixEpoch(1)));
  ASSERT_TRUE(CreateEmptyFile(".ignore/2.jpg",
                              base::Time::FromSecondsSinceUnixEpoch(2)));
  ASSERT_TRUE(
      CreateEmptyFile("3.jpg", base::Time::FromSecondsSinceUnixEpoch(3)));
  ASSERT_TRUE(
      CreateEmptyFile(".4.jpg", base::Time::FromSecondsSinceUnixEpoch(4)));
  // Newest

  std::vector<RecentFile> files = GetRecentFiles(TestParams().MaxFiles(4));

  ASSERT_EQ(4u, files.size());
  EXPECT_EQ(".4.jpg", files[0].url().path().BaseName().value());
  EXPECT_EQ(base::Time::FromSecondsSinceUnixEpoch(4), files[0].last_modified());
  EXPECT_EQ("3.jpg", files[1].url().path().BaseName().value());
  EXPECT_EQ(base::Time::FromSecondsSinceUnixEpoch(3), files[1].last_modified());
  EXPECT_EQ("2.jpg", files[2].url().path().BaseName().value());
  EXPECT_EQ(base::Time::FromSecondsSinceUnixEpoch(2), files[2].last_modified());
  EXPECT_EQ("1.jpg", files[3].url().path().BaseName().value());
  EXPECT_EQ(base::Time::FromSecondsSinceUnixEpoch(1), files[3].last_modified());

  files = GetRecentFiles(TestParams().IgnoreDotFiles(true).MaxFiles(4));

  ASSERT_EQ(2u, files.size());
  EXPECT_EQ("3.jpg", files[0].url().path().BaseName().value());
  EXPECT_EQ(base::Time::FromSecondsSinceUnixEpoch(3), files[0].last_modified());
  EXPECT_EQ("1.jpg", files[1].url().path().BaseName().value());
  EXPECT_EQ(base::Time::FromSecondsSinceUnixEpoch(1), files[1].last_modified());
}

TEST_F(RecentDiskSourceTest, MaxDepth) {
  ASSERT_TRUE(base::CreateDirectory(temp_dir_.GetPath().Append("a")));
  ASSERT_TRUE(base::CreateDirectory(temp_dir_.GetPath().Append("a/b")));
  ASSERT_TRUE(base::CreateDirectory(temp_dir_.GetPath().Append("a/b/c")));

  // Oldest
  ASSERT_TRUE(
      CreateEmptyFile("1.jpg", base::Time::FromSecondsSinceUnixEpoch(1)));
  ASSERT_TRUE(
      CreateEmptyFile("a/2.jpg", base::Time::FromSecondsSinceUnixEpoch(2)));
  ASSERT_TRUE(
      CreateEmptyFile("a/b/3.jpg", base::Time::FromSecondsSinceUnixEpoch(3)));
  ASSERT_TRUE(
      CreateEmptyFile("a/b/c/4.jpg", base::Time::FromSecondsSinceUnixEpoch(4)));
  // Newest

  std::vector<RecentFile> files = GetRecentFiles(TestParams().MaxFiles(4));
  ASSERT_EQ(4u, files.size());

  files = GetRecentFiles(TestParams().MaxFiles(4).MaxDepth(2));

  ASSERT_EQ(2u, files.size());
  EXPECT_EQ("2.jpg", files[0].url().path().BaseName().value());
  EXPECT_EQ(base::Time::FromSecondsSinceUnixEpoch(2), files[0].last_modified());
  EXPECT_EQ("1.jpg", files[1].url().path().BaseName().value());
  EXPECT_EQ(base::Time::FromSecondsSinceUnixEpoch(1), files[1].last_modified());
}

TEST_F(RecentDiskSourceTest, GetAudioFiles) {
  // Oldest
  ASSERT_TRUE(
      CreateEmptyFile("1.jpg", base::Time::FromSecondsSinceUnixEpoch(1)));
  ASSERT_TRUE(
      CreateEmptyFile("2.mp4", base::Time::FromSecondsSinceUnixEpoch(2)));
  ASSERT_TRUE(
      CreateEmptyFile("3.png", base::Time::FromSecondsSinceUnixEpoch(3)));
  ASSERT_TRUE(
      CreateEmptyFile("4.mp3", base::Time::FromSecondsSinceUnixEpoch(4)));
  ASSERT_TRUE(
      CreateEmptyFile("5.gif", base::Time::FromSecondsSinceUnixEpoch(5)));
  ASSERT_TRUE(
      CreateEmptyFile("6.webm", base::Time::FromSecondsSinceUnixEpoch(6)));
  ASSERT_TRUE(
      CreateEmptyFile("7.amr", base::Time::FromSecondsSinceUnixEpoch(7)));
  // Newest

  std::vector<RecentFile> files =
      GetRecentFiles(TestParams().FileType(RecentSource::FileType::kAudio));

  ASSERT_EQ(2u, files.size());
  EXPECT_EQ("7.amr", files[0].url().path().BaseName().value());
  EXPECT_EQ(base::Time::FromSecondsSinceUnixEpoch(7), files[0].last_modified());
  EXPECT_EQ("4.mp3", files[1].url().path().BaseName().value());
  EXPECT_EQ(base::Time::FromSecondsSinceUnixEpoch(4), files[1].last_modified());

  files = GetRecentFiles(
      TestParams().Query("7").FileType(RecentSource::FileType::kAudio));
  ASSERT_EQ(1u, files.size());
  EXPECT_EQ("7.amr", files[0].url().path().BaseName().value());

  files = GetRecentFiles(
      TestParams().Query("6").FileType(RecentSource::FileType::kAudio));
  ASSERT_EQ(0u, files.size());
}

TEST_F(RecentDiskSourceTest, GetImageFiles) {
  // Oldest
  ASSERT_TRUE(
      CreateEmptyFile("1.jpg", base::Time::FromSecondsSinceUnixEpoch(1)));
  ASSERT_TRUE(
      CreateEmptyFile("2.mp4", base::Time::FromSecondsSinceUnixEpoch(2)));
  ASSERT_TRUE(
      CreateEmptyFile("3.png", base::Time::FromSecondsSinceUnixEpoch(3)));
  ASSERT_TRUE(
      CreateEmptyFile("4.mp3", base::Time::FromSecondsSinceUnixEpoch(4)));
  ASSERT_TRUE(
      CreateEmptyFile("5.gif", base::Time::FromSecondsSinceUnixEpoch(5)));
  ASSERT_TRUE(
      CreateEmptyFile("6.webm", base::Time::FromSecondsSinceUnixEpoch(6)));
  // RAW images are supported
  ASSERT_TRUE(
      CreateEmptyFile("7.dng", base::Time::FromSecondsSinceUnixEpoch(7)));
  ASSERT_TRUE(
      CreateEmptyFile("8.nef", base::Time::FromSecondsSinceUnixEpoch(8)));
  // Newest

  std::vector<RecentFile> files =
      GetRecentFiles(TestParams().FileType(RecentSource::FileType::kImage));

  ASSERT_EQ(5u, files.size());
  EXPECT_EQ("8.nef", files[0].url().path().BaseName().value());
  EXPECT_EQ(base::Time::FromSecondsSinceUnixEpoch(8), files[0].last_modified());
  EXPECT_EQ("7.dng", files[1].url().path().BaseName().value());
  EXPECT_EQ(base::Time::FromSecondsSinceUnixEpoch(7), files[1].last_modified());
  EXPECT_EQ("5.gif", files[2].url().path().BaseName().value());
  EXPECT_EQ(base::Time::FromSecondsSinceUnixEpoch(5), files[2].last_modified());
  EXPECT_EQ("3.png", files[3].url().path().BaseName().value());
  EXPECT_EQ(base::Time::FromSecondsSinceUnixEpoch(3), files[3].last_modified());
  EXPECT_EQ("1.jpg", files[4].url().path().BaseName().value());
  EXPECT_EQ(base::Time::FromSecondsSinceUnixEpoch(1), files[4].last_modified());
}

TEST_F(RecentDiskSourceTest, GetVideoFiles) {
  // Oldest
  ASSERT_TRUE(
      CreateEmptyFile("1.jpg", base::Time::FromSecondsSinceUnixEpoch(1)));
  ASSERT_TRUE(
      CreateEmptyFile("2.mp4", base::Time::FromSecondsSinceUnixEpoch(2)));
  ASSERT_TRUE(
      CreateEmptyFile("3.png", base::Time::FromSecondsSinceUnixEpoch(3)));
  ASSERT_TRUE(
      CreateEmptyFile("4.mp3", base::Time::FromSecondsSinceUnixEpoch(4)));
  ASSERT_TRUE(
      CreateEmptyFile("5.gif", base::Time::FromSecondsSinceUnixEpoch(5)));
  ASSERT_TRUE(
      CreateEmptyFile("6.webm", base::Time::FromSecondsSinceUnixEpoch(6)));
  ASSERT_TRUE(
      CreateEmptyFile("7.avi", base::Time::FromSecondsSinceUnixEpoch(7)));
  ASSERT_TRUE(
      CreateEmptyFile("8.mov", base::Time::FromSecondsSinceUnixEpoch(8)));
  // *.wmv is not supported yet.
  ASSERT_TRUE(
      CreateEmptyFile("9.wmv", base::Time::FromSecondsSinceUnixEpoch(9)));
  // Newest

  std::vector<RecentFile> files =
      GetRecentFiles(TestParams().FileType(RecentSource::FileType::kVideo));

  ASSERT_EQ(4u, files.size());
  EXPECT_EQ("8.mov", files[0].url().path().BaseName().value());
  EXPECT_EQ(base::Time::FromSecondsSinceUnixEpoch(8), files[0].last_modified());
  EXPECT_EQ("7.avi", files[1].url().path().BaseName().value());
  EXPECT_EQ(base::Time::FromSecondsSinceUnixEpoch(7), files[1].last_modified());
  EXPECT_EQ("6.webm", files[2].url().path().BaseName().value());
  EXPECT_EQ(base::Time::FromSecondsSinceUnixEpoch(6), files[2].last_modified());
  EXPECT_EQ("2.mp4", files[3].url().path().BaseName().value());
  EXPECT_EQ(base::Time::FromSecondsSinceUnixEpoch(2), files[3].last_modified());
}

TEST_F(RecentDiskSourceTest, GetDocumentFiles) {
  // Oldest
  ASSERT_TRUE(
      CreateEmptyFile("1.jpg", base::Time::FromSecondsSinceUnixEpoch(1)));
  ASSERT_TRUE(
      CreateEmptyFile("2.mp4", base::Time::FromSecondsSinceUnixEpoch(2)));
  ASSERT_TRUE(
      CreateEmptyFile("3.png", base::Time::FromSecondsSinceUnixEpoch(3)));
  ASSERT_TRUE(
      CreateEmptyFile("4.doc", base::Time::FromSecondsSinceUnixEpoch(4)));
  ASSERT_TRUE(
      CreateEmptyFile("5.gif", base::Time::FromSecondsSinceUnixEpoch(5)));
  ASSERT_TRUE(
      CreateEmptyFile("6.txt", base::Time::FromSecondsSinceUnixEpoch(6)));
  ASSERT_TRUE(
      CreateEmptyFile("7.avi", base::Time::FromSecondsSinceUnixEpoch(7)));
  ASSERT_TRUE(
      CreateEmptyFile("8.gdoc", base::Time::FromSecondsSinceUnixEpoch(8)));
  // Newest

  std::vector<RecentFile> files =
      GetRecentFiles(TestParams().FileType(RecentSource::FileType::kDocument));

  ASSERT_EQ(3u, files.size());
  EXPECT_EQ("8.gdoc", files[0].url().path().BaseName().value());
  EXPECT_EQ(base::Time::FromSecondsSinceUnixEpoch(8), files[0].last_modified());
  EXPECT_EQ("6.txt", files[1].url().path().BaseName().value());
  EXPECT_EQ(base::Time::FromSecondsSinceUnixEpoch(6), files[1].last_modified());
  EXPECT_EQ("4.doc", files[2].url().path().BaseName().value());
  EXPECT_EQ(base::Time::FromSecondsSinceUnixEpoch(4), files[2].last_modified());
}

TEST_F(RecentDiskSourceTest, GetRecentFiles_UmaStats) {
  base::HistogramTester histogram_tester;

  GetRecentFiles(TestParams());

  histogram_tester.ExpectTotalCount(uma_histogram_name_, 1);
}

}  // namespace

}  // namespace ash
