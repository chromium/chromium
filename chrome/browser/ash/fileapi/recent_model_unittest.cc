// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/fileapi/recent_file.h"
#include "chrome/browser/ash/fileapi/recent_model.h"
#include "chrome/browser/ash/fileapi/recent_model_factory.h"
#include "chrome/browser/ash/fileapi/test/fake_recent_source.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace ash {

namespace {

RecentFile MakeRecentFile(const std::string& name,
                          const base::Time& last_modified) {
  storage::FileSystemURL url = storage::FileSystemURL::CreateForTest(
      blink::StorageKey(), storage::kFileSystemTypeLocal, base::FilePath(name));
  return RecentFile(url, last_modified);
}

std::vector<std::unique_ptr<RecentSource>> BuildDefaultSourcesWithLag(
    uint32_t lag1_ms,
    uint32_t lag2_ms) {
  auto source1 = std::make_unique<FakeRecentSource>();
  source1->SetLag(base::Milliseconds(lag1_ms));
  source1->AddFile(
      MakeRecentFile("aaa.jpg", base::Time::FromSecondsSinceUnixEpoch(1)));
  source1->AddFile(
      MakeRecentFile("ccc.mp4", base::Time::FromSecondsSinceUnixEpoch(3)));

  auto source2 = std::make_unique<FakeRecentSource>();
  source2->SetLag(base::Milliseconds(lag2_ms));
  source2->AddFile(
      MakeRecentFile("bbb.png", base::Time::FromSecondsSinceUnixEpoch(2)));
  source2->AddFile(
      MakeRecentFile("ddd.ogg", base::Time::FromSecondsSinceUnixEpoch(4)));

  std::vector<std::unique_ptr<RecentSource>> sources;
  sources.emplace_back(std::move(source1));
  sources.emplace_back(std::move(source2));
  return sources;
}

std::vector<std::unique_ptr<RecentSource>> BuildDefaultSources() {
  return BuildDefaultSourcesWithLag(0, 0);
}

std::vector<RecentFile> GetRecentFiles(RecentModel* model,
                                       RecentModel::FileType file_type,
                                       bool invalidate_cache) {
  std::vector<RecentFile> files;

  base::RunLoop run_loop;

  model->GetRecentFiles(
      nullptr /* file_system_context */, GURL() /* origin */,
      "" /* query: unused */, file_type, invalidate_cache,
      base::BindOnce(
          [](base::RunLoop* run_loop, std::vector<RecentFile>* files_out,
             const std::vector<RecentFile>& files) {
            *files_out = files;
            run_loop->Quit();
          },
          &run_loop, &files));

  run_loop.Run();

  return files;
}

}  // namespace

class RecentModelTest : public testing::Test {
 public:
  RecentModelTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

 protected:
  using RecentSourceList = std::vector<std::unique_ptr<RecentSource>>;
  using RecentSourceListFactory = base::RepeatingCallback<RecentSourceList()>;

  std::vector<RecentFile> BuildModelAndGetRecentFiles(
      RecentSourceListFactory source_list_factory,
      size_t max_files,
      const base::Time& cutoff_time,
      RecentModel::FileType file_type,
      bool invalidate_cache) {
    RecentModel* model = static_cast<RecentModel*>(
        RecentModelFactory::GetInstance()->SetTestingFactoryAndUse(
            &profile_,
            base::BindRepeating(
                [](const RecentSourceListFactory& source_list_factory,
                   content::BrowserContext* context)
                    -> std::unique_ptr<KeyedService> {
                  return RecentModel::CreateForTest(source_list_factory.Run());
                },
                std::move(source_list_factory))));

    model->SetMaxFilesForTest(max_files);
    model->SetForcedCutoffTimeForTest(cutoff_time);
    model->SetScanTimeout(base::Milliseconds(500));

    return GetRecentFiles(model, file_type, invalidate_cache);
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(RecentModelTest, GetRecentFiles) {
  std::vector<RecentFile> files = BuildModelAndGetRecentFiles(
      base::BindRepeating(&BuildDefaultSources), 10, base::Time(),
      RecentModel::FileType::kAll, false);

  ASSERT_EQ(4u, files.size());
  EXPECT_EQ("ddd.ogg", files[0].url().path().value());
  EXPECT_EQ(base::Time::FromSecondsSinceUnixEpoch(4), files[0].last_modified());
  EXPECT_EQ("ccc.mp4", files[1].url().path().value());
  EXPECT_EQ(base::Time::FromSecondsSinceUnixEpoch(3), files[1].last_modified());
  EXPECT_EQ("bbb.png", files[2].url().path().value());
  EXPECT_EQ(base::Time::FromSecondsSinceUnixEpoch(2), files[2].last_modified());
  EXPECT_EQ("aaa.jpg", files[3].url().path().value());
  EXPECT_EQ(base::Time::FromSecondsSinceUnixEpoch(1), files[3].last_modified());
}

TEST_F(RecentModelTest, GetRecentFiles_MaxFiles) {
  std::vector<RecentFile> files = BuildModelAndGetRecentFiles(
      base::BindRepeating(&BuildDefaultSources), 3, base::Time(),
      RecentModel::FileType::kAll, false);

  ASSERT_EQ(3u, files.size());
  EXPECT_EQ("ddd.ogg", files[0].url().path().value());
  EXPECT_EQ(base::Time::FromSecondsSinceUnixEpoch(4), files[0].last_modified());
  EXPECT_EQ("ccc.mp4", files[1].url().path().value());
  EXPECT_EQ(base::Time::FromSecondsSinceUnixEpoch(3), files[1].last_modified());
  EXPECT_EQ("bbb.png", files[2].url().path().value());
  EXPECT_EQ(base::Time::FromSecondsSinceUnixEpoch(2), files[2].last_modified());
}

TEST_F(RecentModelTest, GetRecentFiles_CutoffTime) {
  std::vector<RecentFile> files = BuildModelAndGetRecentFiles(
      base::BindRepeating(&BuildDefaultSources), 10,
      base::Time::FromMillisecondsSinceUnixEpoch(2500),
      RecentModel::FileType::kAll, false);

  ASSERT_EQ(2u, files.size());
  EXPECT_EQ("ddd.ogg", files[0].url().path().value());
  EXPECT_EQ(base::Time::FromSecondsSinceUnixEpoch(4), files[0].last_modified());
  EXPECT_EQ("ccc.mp4", files[1].url().path().value());
  EXPECT_EQ(base::Time::FromSecondsSinceUnixEpoch(3), files[1].last_modified());
}

TEST_F(RecentModelTest, GetRecentFiles_UmaStats) {
  base::HistogramTester histogram_tester;

  BuildModelAndGetRecentFiles(
      base::BindRepeating([]() { return RecentSourceList(); }), 10,
      base::Time(), RecentModel::FileType::kAll, false);

  histogram_tester.ExpectTotalCount(RecentModel::kLoadHistogramName, 1);
}

TEST_F(RecentModelTest, GetRecentFiles_Audio) {
  std::vector<RecentFile> files = BuildModelAndGetRecentFiles(
      base::BindRepeating(&BuildDefaultSources), 10, base::Time(),
      RecentModel::FileType::kAudio, false);

  ASSERT_EQ(1u, files.size());
  EXPECT_EQ("ddd.ogg", files[0].url().path().value());
  EXPECT_EQ(base::Time::FromSecondsSinceUnixEpoch(4), files[0].last_modified());
}

TEST_F(RecentModelTest, GetRecentFiles_Image) {
  std::vector<RecentFile> files = BuildModelAndGetRecentFiles(
      base::BindRepeating(&BuildDefaultSources), 10, base::Time(),
      RecentModel::FileType::kImage, false);

  ASSERT_EQ(2u, files.size());
  EXPECT_EQ("bbb.png", files[0].url().path().value());
  EXPECT_EQ(base::Time::FromSecondsSinceUnixEpoch(2), files[0].last_modified());
  EXPECT_EQ("aaa.jpg", files[1].url().path().value());
  EXPECT_EQ(base::Time::FromSecondsSinceUnixEpoch(1), files[1].last_modified());
}

TEST_F(RecentModelTest, GetRecentFiles_Video) {
  std::vector<RecentFile> files = BuildModelAndGetRecentFiles(
      base::BindRepeating(&BuildDefaultSources), 10, base::Time(),
      RecentModel::FileType::kVideo, false);

  ASSERT_EQ(1u, files.size());
  EXPECT_EQ("ccc.mp4", files[0].url().path().value());
  EXPECT_EQ(base::Time::FromSecondsSinceUnixEpoch(3), files[0].last_modified());
}

TEST_F(RecentModelTest, GetRecentFiles_OneSourceIsLate) {
  std::vector<RecentFile> files = BuildModelAndGetRecentFiles(
      base::BindRepeating(&BuildDefaultSourcesWithLag, 100, 501), 10,
      base::Time(), RecentModel::FileType::kAll, false);

  ASSERT_EQ(2u, files.size());
  EXPECT_EQ("ccc.mp4", files[0].url().path().value());
  EXPECT_EQ(base::Time::FromSecondsSinceUnixEpoch(3), files[0].last_modified());
  EXPECT_EQ("aaa.jpg", files[1].url().path().value());
  EXPECT_EQ(base::Time::FromSecondsSinceUnixEpoch(1), files[1].last_modified());
}

TEST_F(RecentModelTest, GetRecentFiles_NoSourceIsLate) {
  std::vector<RecentFile> files = BuildModelAndGetRecentFiles(
      base::BindRepeating(&BuildDefaultSourcesWithLag, 499, 111), 10,
      base::Time(), RecentModel::FileType::kAll, false);

  ASSERT_EQ(4u, files.size());
  EXPECT_EQ("ddd.ogg", files[0].url().path().value());
  EXPECT_EQ(base::Time::FromSecondsSinceUnixEpoch(4), files[0].last_modified());
  EXPECT_EQ("ccc.mp4", files[1].url().path().value());
  EXPECT_EQ(base::Time::FromSecondsSinceUnixEpoch(3), files[1].last_modified());
  EXPECT_EQ("bbb.png", files[2].url().path().value());
  EXPECT_EQ(base::Time::FromSecondsSinceUnixEpoch(2), files[2].last_modified());
  EXPECT_EQ("aaa.jpg", files[3].url().path().value());
  EXPECT_EQ(base::Time::FromSecondsSinceUnixEpoch(1), files[3].last_modified());
}

// Do not use RecentModelTest fixture, because we need to get a reference of
// RecentModel and call GetRecentFiles() multiple times.
TEST(RecentModelCacheTest, GetRecentFiles_InvalidateCache) {
  content::BrowserTaskEnvironment task_environment;
  std::unique_ptr<RecentModel> model =
      RecentModel::CreateForTest(BuildDefaultSources());
  model->SetForcedCutoffTimeForTest(base::Time());

  std::vector<RecentFile> files1 =
      GetRecentFiles(model.get(), RecentModel::FileType::kAll, false);
  ASSERT_EQ(4u, files1.size());

  // Shutdown() will clear all sources.
  model->Shutdown();

  // The returned file list should still has 4 files even though all sources has
  // been cleared in Shutdown(), because it hits cache.
  std::vector<RecentFile> files2 =
      GetRecentFiles(model.get(), RecentModel::FileType::kAll, false);
  ASSERT_EQ(4u, files2.size());

  // Inalidate cache and query again.
  std::vector<RecentFile> files3 =
      GetRecentFiles(model.get(), RecentModel::FileType::kAll, true);
  ASSERT_EQ(0u, files3.size());
}

}  // namespace ash
