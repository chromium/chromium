// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/fileapi/recent_model.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/fileapi/recent_file.h"
#include "chrome/browser/ash/fileapi/recent_model_factory.h"
#include "chrome/browser/ash/fileapi/test/fake_recent_source.h"
#include "chrome/browser/ash/fileapi/test/recent_file_matcher.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace ash {

namespace {

namespace fmp = extensions::api::file_manager_private;

base::Time ModifiedTime(int64_t seconds_since_unix_epoch) {
  return base::Time::FromSecondsSinceUnixEpoch(seconds_since_unix_epoch);
}

base::FilePath FilePath(const char* name) {
  return base::FilePath(name);
}

RecentFile MakeRecentFile(const base::FilePath& path,
                          const base::Time& last_modified) {
  storage::FileSystemURL url = storage::FileSystemURL::CreateForTest(
      blink::StorageKey(), storage::kFileSystemTypeLocal, path);
  return RecentFile(url, last_modified);
}

std::unique_ptr<RecentSource> MakeRecentSource(
    uint32_t lag_ms,
    const std::vector<RecentFile> files) {
  auto source = std::make_unique<FakeRecentSource>();
  if (lag_ms == 0) {
    source->AddProducer(std::make_unique<FileProducer>(
        base::Milliseconds(0),
        std::initializer_list<RecentFile>{files[0], files[1]}));
  } else {
    source->AddProducer(std::make_unique<FileProducer>(
        base::Milliseconds(lag_ms),
        std::initializer_list<RecentFile>{files[0]}));
    source->AddProducer(std::make_unique<FileProducer>(
        base::Milliseconds(2 * lag_ms),
        std::initializer_list<RecentFile>{files[1]}));
  }
  return source;
}

// A helper method that generates two recent sources. If lag is set to 0 all
// files of the recent source are delivered at once at 0ms delay. Otherwise,
// one file is delivered with the given lag, and the other with twice the lag.
// If, say, one was to call this method with lags 100ms and 150ms, this would
// allow one to stagger file delivery as follows:
//
//       0ms      100ms     150ms     200ms     300ms
//  s1:           aaa.jpg             ccc.mp4
//  s2:                     bbb.png             ddd.ogg
std::vector<std::unique_ptr<RecentSource>> BuildDefaultSourcesWithLag(
    uint32_t lag1_ms,
    uint32_t lag2_ms) {
  RecentFile files[] = {
      // Source 1 files:
      MakeRecentFile(FilePath("aaa.jpg"), ModifiedTime(1)),
      MakeRecentFile(FilePath("ccc.mp4"), ModifiedTime(3)),
      // Source 2 files:
      MakeRecentFile(FilePath("bbb.png"), ModifiedTime(2)),
      MakeRecentFile(FilePath("ddd.ogg"), ModifiedTime(4)),
  };
  std::vector<std::unique_ptr<RecentSource>> sources;
  sources.emplace_back(MakeRecentSource(lag1_ms, {files[0], files[1]}));
  sources.emplace_back(MakeRecentSource(lag2_ms, {files[2], files[3]}));
  return sources;
}

std::vector<std::unique_ptr<RecentSource>> BuildDefaultSources() {
  return BuildDefaultSourcesWithLag(0, 0);
}

std::vector<RecentFile> GetRecentFiles(RecentModel* model,
                                       const RecentModelOptions& options) {
  std::vector<RecentFile> files;

  base::RunLoop run_loop;

  model->GetRecentFiles(
      /*file_system_context=*/nullptr, /*origin=*/GURL(),
      /*query=*/"", options,
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

  void SetUp() override {
    source_specs_.emplace_back(
        RecentSourceSpec{.volume_type = fmp::VolumeType::kTesting});
  }

  void TearDown() override { source_specs_.clear(); }

 protected:
  using RecentSourceList = std::vector<std::unique_ptr<RecentSource>>;
  using RecentSourceListFactory = base::RepeatingCallback<RecentSourceList()>;

  RecentModel* CreateRecentModel(RecentSourceListFactory source_list_factory) {
    return static_cast<RecentModel*>(
        RecentModelFactory::GetInstance()->SetTestingFactoryAndUse(
            &profile_,
            base::BindRepeating(
                [](const RecentSourceListFactory& source_list_factory,
                   content::BrowserContext* context)
                    -> std::unique_ptr<KeyedService> {
                  return RecentModel::CreateForTest(source_list_factory.Run());
                },
                std::move(source_list_factory))));
  }

  std::vector<RecentFile> BuildModelAndGetRecentFiles(
      RecentSourceListFactory source_list_factory,
      size_t max_files,
      const base::TimeDelta& cutoff_delta,
      RecentModel::FileType file_type,
      bool invalidate_cache) {
    RecentModel* model = CreateRecentModel(source_list_factory);
    RecentModelOptions options;
    options.scan_timeout = base::Milliseconds(500);
    options.max_files = max_files;
    options.file_type = file_type;
    options.invalidate_cache = invalidate_cache;
    options.now_delta = cutoff_delta;
    options.source_specs = source_specs_;

    return GetRecentFiles(model, options);
  }

  std::vector<RecentSourceSpec> source_specs_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(RecentModelTest, GetRecentFiles) {
  std::vector<RecentFile> files = BuildModelAndGetRecentFiles(
      base::BindRepeating(&BuildDefaultSources), 10, base::Days(30),
      RecentModel::FileType::kAll, false);

  ASSERT_EQ(4u, files.size());
  EXPECT_THAT(files[0], IsRecentFile(FilePath("ddd.ogg"), ModifiedTime(4)));
  EXPECT_THAT(files[1], IsRecentFile(FilePath("ccc.mp4"), ModifiedTime(3)));
  EXPECT_THAT(files[2], IsRecentFile(FilePath("bbb.png"), ModifiedTime(2)));
  EXPECT_THAT(files[3], IsRecentFile(FilePath("aaa.jpg"), ModifiedTime(1)));
}

TEST_F(RecentModelTest, GetRecentFiles_MaxFiles) {
  std::vector<RecentFile> files = BuildModelAndGetRecentFiles(
      base::BindRepeating(&BuildDefaultSources), 3, base::Days(30),
      RecentModel::FileType::kAll, false);

  ASSERT_EQ(3u, files.size());
  EXPECT_THAT(files[0], IsRecentFile(FilePath("ddd.ogg"), ModifiedTime(4)));
  EXPECT_THAT(files[1], IsRecentFile(FilePath("ccc.mp4"), ModifiedTime(3)));
  EXPECT_THAT(files[2], IsRecentFile(FilePath("bbb.png"), ModifiedTime(2)));
}

TEST_F(RecentModelTest, GetRecentFiles_CutoffTime) {
  // TODO(b:307455066): Fix last modified time in created files.
  // Files created for the tests have last modified time set to 1, 2, 3, and 4
  // seconds since Unix epoch. This allows for last modified time checks to be
  // performed independently of when the test is run. However, this is not
  // ideal. First, there is a repetitive base::Time::FromSecondsSinceUnixEpoch
  // scattered in the tests. Changing, say, the last modified time for aaa.jpg
  // would require changing it everywhere in the tests. In addition, now that
  // one can pass time delta to GetRecentFiles method, this makes testing
  // fragile as we need to hit the right span of a few seconds between now and
  // the start of the Unix epoch.
  std::vector<RecentFile> files = BuildModelAndGetRecentFiles(
      base::BindRepeating(&BuildDefaultSources), 10,
      base::Time::Now() - base::Time::FromMillisecondsSinceUnixEpoch(2500),
      RecentModel::FileType::kAll, false);

  ASSERT_EQ(2u, files.size());
  EXPECT_THAT(files[0], IsRecentFile(FilePath("ddd.ogg"), ModifiedTime(4)));
  EXPECT_THAT(files[1], IsRecentFile(FilePath("ccc.mp4"), ModifiedTime(3)));
}

TEST_F(RecentModelTest, GetRecentFiles_UmaStats) {
  base::HistogramTester histogram_tester;

  BuildModelAndGetRecentFiles(
      base::BindRepeating([]() { return RecentSourceList(); }), 10,
      base::Days(30), RecentModel::FileType::kAll, false);

  histogram_tester.ExpectTotalCount(RecentModel::kLoadHistogramName, 1);
}

TEST_F(RecentModelTest, GetRecentFiles_Audio) {
  std::vector<RecentFile> files = BuildModelAndGetRecentFiles(
      base::BindRepeating(&BuildDefaultSources), 10, base::Days(30),
      RecentModel::FileType::kAudio, false);

  ASSERT_EQ(1u, files.size());
  EXPECT_THAT(files[0], IsRecentFile(FilePath("ddd.ogg"), ModifiedTime(4)));
}

TEST_F(RecentModelTest, GetRecentFiles_Image) {
  std::vector<RecentFile> files = BuildModelAndGetRecentFiles(
      base::BindRepeating(&BuildDefaultSources), 10, base::Days(30),
      RecentModel::FileType::kImage, false);

  ASSERT_EQ(2u, files.size());
  EXPECT_THAT(files[0], IsRecentFile(FilePath("bbb.png"), ModifiedTime(2)));
  EXPECT_THAT(files[1], IsRecentFile(FilePath("aaa.jpg"), ModifiedTime(1)));
}

TEST_F(RecentModelTest, GetRecentFiles_Video) {
  std::vector<RecentFile> files = BuildModelAndGetRecentFiles(
      base::BindRepeating(&BuildDefaultSources), 10, base::Days(30),
      RecentModel::FileType::kVideo, false);

  ASSERT_EQ(1u, files.size());
  EXPECT_THAT(files[0], IsRecentFile(FilePath("ccc.mp4"), ModifiedTime(3)));
}

TEST_F(RecentModelTest, GetRecentFiles_OneSourceIsLate) {
  std::vector<RecentFile> files = BuildModelAndGetRecentFiles(
      base::BindRepeating(&BuildDefaultSourcesWithLag, 100, 501), 10,
      base::Days(30), RecentModel::FileType::kAll, false);

  ASSERT_EQ(2u, files.size());
  EXPECT_THAT(files[0], IsRecentFile(FilePath("ccc.mp4"), ModifiedTime(3)));
  EXPECT_THAT(files[1], IsRecentFile(FilePath("aaa.jpg"), ModifiedTime(1)));
}

TEST_F(RecentModelTest, GetRecentFiles_FirstSourceIsPartiallyLate) {
  std::vector<RecentFile> files = BuildModelAndGetRecentFiles(
      base::BindRepeating(&BuildDefaultSourcesWithLag, 499, 111), 10,
      base::Days(30), RecentModel::FileType::kAll, false);

  ASSERT_EQ(3u, files.size());
  EXPECT_THAT(files[0], IsRecentFile(FilePath("ddd.ogg"), ModifiedTime(4)));
  EXPECT_THAT(files[1], IsRecentFile(FilePath("bbb.png"), ModifiedTime(2)));
  EXPECT_THAT(files[2], IsRecentFile(FilePath("aaa.jpg"), ModifiedTime(1)));
}

TEST_F(RecentModelTest, GetRecentFiles_SecondSourceIsPartiallyLate) {
  std::vector<RecentFile> files = BuildModelAndGetRecentFiles(
      base::BindRepeating(&BuildDefaultSourcesWithLag, 50, 251), 10,
      base::Days(30), RecentModel::FileType::kAll, false);

  ASSERT_EQ(3u, files.size());
  EXPECT_THAT(files[0], IsRecentFile(FilePath("ccc.mp4"), ModifiedTime(3)));
  EXPECT_THAT(files[1], IsRecentFile(FilePath("bbb.png"), ModifiedTime(2)));
  EXPECT_THAT(files[2], IsRecentFile(FilePath("aaa.jpg"), ModifiedTime(1)));
}

TEST_F(RecentModelTest, GetRecentFiles_NoSourceIsLate) {
  std::vector<RecentFile> files = BuildModelAndGetRecentFiles(
      base::BindRepeating(&BuildDefaultSourcesWithLag, 249, 111), 10,
      base::Days(30), RecentModel::FileType::kAll, false);

  ASSERT_EQ(4u, files.size());
  EXPECT_THAT(files[0], IsRecentFile(FilePath("ddd.ogg"), ModifiedTime(4)));
  EXPECT_THAT(files[1], IsRecentFile(FilePath("ccc.mp4"), ModifiedTime(3)));
  EXPECT_THAT(files[2], IsRecentFile(FilePath("bbb.png"), ModifiedTime(2)));
  EXPECT_THAT(files[3], IsRecentFile(FilePath("aaa.jpg"), ModifiedTime(1)));
}

TEST_F(RecentModelTest, GetRecentFiles_AllSourcesAreLate) {
  std::vector<RecentFile> files = BuildModelAndGetRecentFiles(
      base::BindRepeating(&BuildDefaultSourcesWithLag, 501, 502), 10,
      base::Days(30), RecentModel::FileType::kAll, false);

  ASSERT_EQ(0u, files.size());
}

// Checks the behavior of the recent model if we have two requests issued with
// different queries at the almost same time.
TEST_F(RecentModelTest, MultipleRequests) {
  // Creates laggy sources, so that we can call GetRecentFiles twice.
  RecentModel* model = CreateRecentModel(
      base::BindRepeating(&BuildDefaultSourcesWithLag, 500, 500));

  std::vector<RecentFile> files_1;
  std::vector<RecentFile> files_2;
  base::RunLoop loop;
  int calls_completed_count = 0;

  // First request; fills files_1. We use query "aaa"
  RecentModelOptions options;
  options.max_files = 10;
  options.source_specs = source_specs_;
  model->GetRecentFiles(
      /*file_system_context=*/nullptr, /*origin=*/GURL(),
      /*query=*/"aaa", options,
      base::BindLambdaForTesting([&](const std::vector<RecentFile>& files) {
        files_1 = files;
        if (++calls_completed_count == 2) {
          loop.Quit();
        }
      }));

  // Use a different query, expect different results.
  model->GetRecentFiles(
      /*file_system_context=*/nullptr, /*origin=*/GURL(),
      /*query=*/"bbb", options,
      base::BindLambdaForTesting([&](const std::vector<RecentFile>& files) {
        files_2 = files;
        if (++calls_completed_count == 2) {
          loop.Quit();
        }
      }));

  loop.Run();

  ASSERT_EQ(1u, files_1.size());
  EXPECT_THAT(files_1[0], IsRecentFile(FilePath("aaa.jpg"), ModifiedTime(1)));

  ASSERT_EQ(1u, files_2.size());
  EXPECT_THAT(files_2[0], IsRecentFile(FilePath("bbb.png"), ModifiedTime(2)));
}

// Do not use RecentModelTest fixture, because we need to get a reference of
// RecentModel and call GetRecentFiles() multiple times.
TEST(RecentModelCacheTest, GetRecentFiles_InvalidateCache) {
  content::BrowserTaskEnvironment task_environment;
  std::unique_ptr<RecentModel> model =
      RecentModel::CreateForTest(BuildDefaultSources());

  RecentModelOptions options;
  options.max_files = 10;
  options.now_delta = base::TimeDelta::Max();
  options.source_specs.emplace_back(
      RecentSourceSpec{.volume_type = fmp::VolumeType::kTesting});
  std::vector<RecentFile> files1 = GetRecentFiles(model.get(), options);
  ASSERT_EQ(4u, files1.size());

  // Shutdown() will clear all sources.
  model->Shutdown();

  // The returned file list should still has 4 files even though all sources has
  // been cleared in Shutdown(), because it hits cache.
  std::vector<RecentFile> files2 = GetRecentFiles(model.get(), options);
  ASSERT_EQ(4u, files2.size());

  // Inalidate cache and query again.
  options.invalidate_cache = true;
  std::vector<RecentFile> files3 = GetRecentFiles(model.get(), options);
  ASSERT_EQ(0u, files3.size());
}

TEST(RecentModelSourceRestrictions, QueryNonexistingSources) {
  content::BrowserTaskEnvironment task_environment;
  std::unique_ptr<RecentModel> model =
      RecentModel::CreateForTest(BuildDefaultSources());

  RecentModelOptions options;
  options.max_files = 10;
  options.now_delta = base::TimeDelta::Max();
  options.source_specs.emplace_back(
      RecentSourceSpec{.volume_type = fmp::VolumeType::kDrive});
  options.source_specs.emplace_back(
      RecentSourceSpec{.volume_type = fmp::VolumeType::kDownloads});
  std::vector<RecentFile> files = GetRecentFiles(model.get(), options);
  // Test sources have kTesting as the volume type; thus fetching files from
  // other volumes should result in empty recent files vector.
  EXPECT_TRUE(files.empty());
  // Manual shutdown to clear sources_ vector in the model.
  model->Shutdown();
}

}  // namespace ash
