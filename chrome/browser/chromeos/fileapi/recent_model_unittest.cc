// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/chromeos/fileapi/recent_file.h"
#include "chrome/browser/chromeos/fileapi/recent_model.h"
#include "chrome/browser/chromeos/fileapi/recent_model_factory.h"
#include "chrome/browser/chromeos/fileapi/test/fake_recent_source.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

RecentFile MakeRecentFile(const std::string& name,
                          const base::Time& last_modified) {
  storage::FileSystemURL url = storage::FileSystemURL::CreateForTest(
      url::Origin(),  // origin
      storage::kFileSystemTypeNativeLocal, base::FilePath(name));
  return RecentFile(url, last_modified);
}

std::vector<std::unique_ptr<RecentSource>> BuildDefaultSources() {
  auto source1 = std::make_unique<FakeRecentSource>();
  source1->AddFile(MakeRecentFile("aaa.jpg", base::Time::FromJavaTime(1000)));
  source1->AddFile(MakeRecentFile("ccc.jpg", base::Time::FromJavaTime(3000)));

  auto source2 = std::make_unique<FakeRecentSource>();
  source2->AddFile(MakeRecentFile("bbb.jpg", base::Time::FromJavaTime(2000)));
  source2->AddFile(MakeRecentFile("ddd.jpg", base::Time::FromJavaTime(4000)));

  std::vector<std::unique_ptr<RecentSource>> sources;
  sources.emplace_back(std::move(source1));
  sources.emplace_back(std::move(source2));
  return sources;
}

}  // namespace

class RecentModelTest : public testing::Test {
 public:
  RecentModelTest() = default;

 protected:
  using RecentSourceList = std::vector<std::unique_ptr<RecentSource>>;
  using RecentSourceListFactory = base::RepeatingCallback<RecentSourceList()>;

  std::vector<RecentFile> BuildModelAndGetRecentFiles(
      RecentSourceListFactory source_list_factory,
      size_t max_files,
      const base::Time& cutoff_time) {
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

    std::vector<RecentFile> files;

    base::RunLoop run_loop;

    model->GetRecentFiles(
        nullptr /* file_system_context */, GURL() /* origin */,
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

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(RecentModelTest, GetRecentFiles) {
  std::vector<RecentFile> files = BuildModelAndGetRecentFiles(
      base::BindRepeating(&BuildDefaultSources), 10, base::Time());

  ASSERT_EQ(4u, files.size());
  EXPECT_EQ("ddd.jpg", files[0].url().path().value());
  EXPECT_EQ(base::Time::FromJavaTime(4000), files[0].last_modified());
  EXPECT_EQ("ccc.jpg", files[1].url().path().value());
  EXPECT_EQ(base::Time::FromJavaTime(3000), files[1].last_modified());
  EXPECT_EQ("bbb.jpg", files[2].url().path().value());
  EXPECT_EQ(base::Time::FromJavaTime(2000), files[2].last_modified());
  EXPECT_EQ("aaa.jpg", files[3].url().path().value());
  EXPECT_EQ(base::Time::FromJavaTime(1000), files[3].last_modified());
}

TEST_F(RecentModelTest, GetRecentFiles_MaxFiles) {
  std::vector<RecentFile> files = BuildModelAndGetRecentFiles(
      base::BindRepeating(&BuildDefaultSources), 3, base::Time());

  ASSERT_EQ(3u, files.size());
  EXPECT_EQ("ddd.jpg", files[0].url().path().value());
  EXPECT_EQ(base::Time::FromJavaTime(4000), files[0].last_modified());
  EXPECT_EQ("ccc.jpg", files[1].url().path().value());
  EXPECT_EQ(base::Time::FromJavaTime(3000), files[1].last_modified());
  EXPECT_EQ("bbb.jpg", files[2].url().path().value());
  EXPECT_EQ(base::Time::FromJavaTime(2000), files[2].last_modified());
}

TEST_F(RecentModelTest, GetRecentFiles_CutoffTime) {
  std::vector<RecentFile> files =
      BuildModelAndGetRecentFiles(base::BindRepeating(&BuildDefaultSources), 10,
                                  base::Time::FromJavaTime(2500));

  ASSERT_EQ(2u, files.size());
  EXPECT_EQ("ddd.jpg", files[0].url().path().value());
  EXPECT_EQ(base::Time::FromJavaTime(4000), files[0].last_modified());
  EXPECT_EQ("ccc.jpg", files[1].url().path().value());
  EXPECT_EQ(base::Time::FromJavaTime(3000), files[1].last_modified());
}

TEST_F(RecentModelTest, GetRecentFiles_UmaStats) {
  base::HistogramTester histogram_tester;

  BuildModelAndGetRecentFiles(
      base::BindRepeating([]() { return RecentSourceList(); }), 10,
      base::Time());

  histogram_tester.ExpectTotalCount(RecentModel::kLoadHistogramName, 1);
}

}  // namespace chromeos
