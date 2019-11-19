// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_engagement_preloaded_list.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

const base::FilePath kTestDataPath = base::FilePath(
    FILE_PATH_LITERAL("gen/chrome/test/data/media/engagement/preload"));

// This sample data is auto generated at build time.
const base::FilePath kSampleDataPath = kTestDataPath.AppendASCII("test.pb");

const base::FilePath kMissingFilePath = kTestDataPath.AppendASCII("missing.pb");

const base::FilePath kBadFormatFilePath =
    kTestDataPath.AppendASCII("bad_format.pb");

const base::FilePath kEmptyFilePath = kTestDataPath.AppendASCII("empty.pb");

const base::FilePath kFileReadFailedPath =
    base::FilePath(FILE_PATH_LITERAL(".."));

base::FilePath GetModulePath() {
  base::FilePath module_dir;
#if defined(OS_ANDROID)
  EXPECT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &module_dir));
#else
  EXPECT_TRUE(base::PathService::Get(base::DIR_MODULE, &module_dir));
#endif
  return module_dir;
}

}  // namespace

class MediaEngagementPreloadedListTest : public ::testing::Test {
 public:
  void SetUp() override {
    preloaded_list_ = std::make_unique<MediaEngagementPreloadedList>();
    ASSERT_FALSE(IsLoaded());
    ASSERT_TRUE(IsEmpty());
  }

  bool LoadFromFile(base::FilePath path) {
    return preloaded_list_->LoadFromFile(path);
  }

  bool CheckOriginIsPresent(GURL url) {
    return preloaded_list_->CheckOriginIsPresent(url::Origin::Create(url));
  }

  bool CheckStringIsPresent(const std::string& input) {
    return preloaded_list_->CheckStringIsPresent(input) !=
           MediaEngagementPreloadedList::DafsaResult::kNotFound;
  }

  base::FilePath GetFilePathRelativeToModule(base::FilePath path) {
    return GetModulePath().Append(path);
  }

  bool IsLoaded() { return preloaded_list_->loaded(); }

  bool IsEmpty() { return preloaded_list_->empty(); }

  void ExpectCheckResultFoundHttpsOnlyCount(int count) {
    ExpectCheckResultCount(
        MediaEngagementPreloadedList::CheckResult::kFoundHttpsOnly, count);
  }

  void ExpectCheckResultFoundHttpsButWasHttpOnlyCount(int count) {
    ExpectCheckResultCount(
        MediaEngagementPreloadedList::CheckResult::kFoundHttpsOnlyButWasHttp,
        count);
  }

  void ExpectCheckResultFoundHttpOrHttpsCount(int count) {
    ExpectCheckResultCount(
        MediaEngagementPreloadedList::CheckResult::kFoundHttpOrHttps, count);
  }

  void ExpectCheckResultNotFoundCount(int count) {
    ExpectCheckResultCount(MediaEngagementPreloadedList::CheckResult::kNotFound,
                           count);
  }

  void ExpectCheckResultNotLoadedCount(int count) {
    ExpectCheckResultCount(
        MediaEngagementPreloadedList::CheckResult::kListNotLoaded, count);
  }

  void ExpectCheckResultListEmptyCount(int count) {
    ExpectCheckResultCount(
        MediaEngagementPreloadedList::CheckResult::kListEmpty, count);
  }

  void ExpectCheckResultTotal(int total) {
    histogram_tester_.ExpectTotalCount(
        MediaEngagementPreloadedList::kHistogramCheckResultName, total);
  }

  void ExpectLoadResultLoaded() {
    ExpectLoadResult(MediaEngagementPreloadedList::LoadResult::kLoaded);
  }

  void ExpectLoadResultFileNotFound() {
    ExpectLoadResult(MediaEngagementPreloadedList::LoadResult::kFileNotFound);
  }

  void ExpectLoadResultFileReadFailed() {
    ExpectLoadResult(MediaEngagementPreloadedList::LoadResult::kFileReadFailed);
  }

  void ExpectLoadResultParseProtoFailed() {
    ExpectLoadResult(
        MediaEngagementPreloadedList::LoadResult::kParseProtoFailed);
  }

  const base::HistogramTester& histogram_tester() const {
    return histogram_tester_;
  }

 protected:
  void ExpectLoadResult(MediaEngagementPreloadedList::LoadResult result) {
    histogram_tester_.ExpectBucketCount(
        MediaEngagementPreloadedList::kHistogramLoadResultName,
        static_cast<int>(result), 1);

    // Ensure not other results were logged.
    histogram_tester_.ExpectTotalCount(
        MediaEngagementPreloadedList::kHistogramLoadResultName, 1);
  }

  void ExpectCheckResultCount(MediaEngagementPreloadedList::CheckResult result,
                              int count) {
    histogram_tester_.ExpectBucketCount(
        MediaEngagementPreloadedList::kHistogramCheckResultName,
        static_cast<int>(result), count);
  }

  std::unique_ptr<MediaEngagementPreloadedList> preloaded_list_;

  base::HistogramTester histogram_tester_;
};

TEST_F(MediaEngagementPreloadedListTest, CheckOriginIsPresent) {
  ASSERT_TRUE(LoadFromFile(GetFilePathRelativeToModule(kSampleDataPath)));
  EXPECT_TRUE(IsLoaded());
  EXPECT_FALSE(IsEmpty());

  // Check the load result was recorded on the histogram.
  ExpectLoadResultLoaded();

  // Check some origins that are not in the list.
  ExpectCheckResultTotal(0);
  EXPECT_TRUE(CheckOriginIsPresent(GURL("https://example.com")));
  EXPECT_TRUE(CheckOriginIsPresent(GURL("https://example.org:1234")));
  EXPECT_TRUE(CheckOriginIsPresent(GURL("https://test--3ya.com")));
  EXPECT_TRUE(CheckOriginIsPresent(GURL("http://123.123.123.123")));

  // Check they were recorded on the histogram.
  ExpectCheckResultTotal(4);
  ExpectCheckResultFoundHttpsOnlyCount(3);
  ExpectCheckResultFoundHttpOrHttpsCount(1);

  // Check some origins that are not in the list.
  EXPECT_FALSE(CheckOriginIsPresent(GURL("https://example.org")));
  EXPECT_FALSE(CheckOriginIsPresent(GURL("http://example.com")));
  EXPECT_FALSE(CheckOriginIsPresent(GURL("http://123.123.123.124")));

  // Check they were recorded on the histogram.
  ExpectCheckResultTotal(7);
  ExpectCheckResultNotFoundCount(2);
  ExpectCheckResultFoundHttpsButWasHttpOnlyCount(1);

  // Make sure only the full origin matches.
  EXPECT_FALSE(CheckStringIsPresent("123"));
  EXPECT_FALSE(CheckStringIsPresent("http"));
  EXPECT_FALSE(CheckStringIsPresent("example.org"));
}

TEST_F(MediaEngagementPreloadedListTest, LoadMissingFile) {
  ASSERT_FALSE(LoadFromFile(GetFilePathRelativeToModule(kMissingFilePath)));
  EXPECT_FALSE(IsLoaded());
  EXPECT_TRUE(IsEmpty());

  // Check the load result was recorded on the histogram.
  ExpectLoadResultFileNotFound();

  // Test checking an origin and make sure the result is recorded to the
  // histogram.
  EXPECT_FALSE(CheckOriginIsPresent(GURL("https://example.com")));
  ExpectCheckResultTotal(1);
  ExpectCheckResultNotLoadedCount(1);
}

TEST_F(MediaEngagementPreloadedListTest, LoadFileReadFailed) {
  ASSERT_FALSE(LoadFromFile(kFileReadFailedPath));
  EXPECT_FALSE(IsLoaded());
  EXPECT_TRUE(IsEmpty());

  // Check the load result was recorded on the histogram.
  ExpectLoadResultFileReadFailed();

  // Test checking an origin and make sure the result is recorded to the
  // histogram.
  EXPECT_FALSE(CheckOriginIsPresent(GURL("https://example.com")));
  ExpectCheckResultTotal(1);
  ExpectCheckResultNotLoadedCount(1);
}

TEST_F(MediaEngagementPreloadedListTest, LoadBadFormatFile) {
  ASSERT_FALSE(LoadFromFile(GetFilePathRelativeToModule(kBadFormatFilePath)));
  EXPECT_FALSE(IsLoaded());
  EXPECT_TRUE(IsEmpty());

  // Check the load result was recorded on the histogram.
  ExpectLoadResultParseProtoFailed();

  // Test checking an origin and make sure the result is recorded to the
  // histogram.
  EXPECT_FALSE(CheckOriginIsPresent(GURL("https://example.com")));
  ExpectCheckResultTotal(1);
  ExpectCheckResultNotLoadedCount(1);
}

TEST_F(MediaEngagementPreloadedListTest, LoadEmptyFile) {
  ASSERT_TRUE(LoadFromFile(GetFilePathRelativeToModule(kEmptyFilePath)));
  EXPECT_TRUE(IsLoaded());
  EXPECT_TRUE(IsEmpty());

  // Check the load result was recorded on the histogram.
  ExpectLoadResultLoaded();

  // Test checking an origin and make sure the result is recorded to the
  // histogram.
  EXPECT_FALSE(CheckOriginIsPresent(GURL("https://example.com")));
  ExpectCheckResultTotal(1);
  ExpectCheckResultListEmptyCount(1);
}

TEST_F(MediaEngagementPreloadedListTest, CheckOriginIsPresent_UnsecureSchemes) {
  ASSERT_TRUE(LoadFromFile(GetFilePathRelativeToModule(kSampleDataPath)));
  EXPECT_TRUE(IsLoaded());
  EXPECT_FALSE(IsEmpty());

  // Check the load result was recorded on the histogram.
  ExpectLoadResultLoaded();

  // An origin that has both HTTP and HTTPS entries should allow either.
  EXPECT_TRUE(CheckOriginIsPresent(GURL("https://google.com")));
  EXPECT_TRUE(CheckOriginIsPresent(GURL("http://google.com")));
  ExpectCheckResultTotal(2);
  ExpectCheckResultFoundHttpOrHttpsCount(2);

  // An origin that only has a HTTP origin should allow either.
  EXPECT_TRUE(CheckOriginIsPresent(GURL("https://123.123.123.123")));
  EXPECT_TRUE(CheckOriginIsPresent(GURL("http://123.123.123.123")));
  ExpectCheckResultTotal(4);
  ExpectCheckResultFoundHttpOrHttpsCount(4);

  // An origin that has only HTTPS should only allow HTTPS.
  EXPECT_TRUE(CheckOriginIsPresent(GURL("https://example.com")));
  EXPECT_FALSE(CheckOriginIsPresent(GURL("http://example.com")));
  ExpectCheckResultTotal(6);
  ExpectCheckResultFoundHttpsOnlyCount(1);
  ExpectCheckResultFoundHttpsButWasHttpOnlyCount(1);
}
