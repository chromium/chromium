// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_engagement_preloaded_list.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

// Generated files are re-homed to the package root.
const base::FilePath kTestDataPath = base::FilePath(
    FILE_PATH_LITERAL("chrome/test/data/media/engagement/preload"));

// This sample data is auto generated at build time.
const base::FilePath kSampleDataPath = kTestDataPath.AppendASCII("test.pb");

const base::FilePath kMissingFilePath = kTestDataPath.AppendASCII("missing.pb");

const base::FilePath kBadFormatFilePath =
    kTestDataPath.AppendASCII("bad_format.pb");

const base::FilePath kEmptyFilePath = kTestDataPath.AppendASCII("empty.pb");

const base::FilePath kFileReadFailedPath =
    base::FilePath(FILE_PATH_LITERAL(".."));

base::FilePath GeneratedTestDataRoot() {
  return base::PathService::CheckedGet(base::DIR_GEN_TEST_DATA_ROOT);
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

  base::FilePath GetAbsolutePathToGeneratedTestFile(base::FilePath path) {
    return GeneratedTestDataRoot().Append(path);
  }

  bool IsLoaded() { return preloaded_list_->loaded(); }

  bool IsEmpty() { return preloaded_list_->empty(); }

  std::unique_ptr<MediaEngagementPreloadedList> preloaded_list_;
};

TEST_F(MediaEngagementPreloadedListTest, CheckOriginIsPresent) {
  ASSERT_TRUE(
      LoadFromFile(GetAbsolutePathToGeneratedTestFile(kSampleDataPath)));
  EXPECT_TRUE(IsLoaded());
  EXPECT_FALSE(IsEmpty());

  // Check some origins that are not in the list.
  EXPECT_TRUE(CheckOriginIsPresent(GURL("https://example.com")));
  EXPECT_TRUE(CheckOriginIsPresent(GURL("https://example.org:1234")));
  EXPECT_TRUE(CheckOriginIsPresent(GURL("https://test--3ya.com")));
  EXPECT_TRUE(CheckOriginIsPresent(GURL("http://123.123.123.123")));

  // Check some origins that are not in the list.
  EXPECT_FALSE(CheckOriginIsPresent(GURL("https://example.org")));
  EXPECT_FALSE(CheckOriginIsPresent(GURL("http://example.com")));
  EXPECT_FALSE(CheckOriginIsPresent(GURL("http://123.123.123.124")));

  // Make sure only the full origin matches.
  EXPECT_FALSE(CheckStringIsPresent("123"));
  EXPECT_FALSE(CheckStringIsPresent("http"));
  EXPECT_FALSE(CheckStringIsPresent("example.org"));
}

TEST_F(MediaEngagementPreloadedListTest, LoadMissingFile) {
  ASSERT_FALSE(
      LoadFromFile(GetAbsolutePathToGeneratedTestFile(kMissingFilePath)));
  EXPECT_FALSE(IsLoaded());
  EXPECT_TRUE(IsEmpty());
}

TEST_F(MediaEngagementPreloadedListTest, LoadFileReadFailed) {
  ASSERT_FALSE(LoadFromFile(kFileReadFailedPath));
  EXPECT_FALSE(IsLoaded());
  EXPECT_TRUE(IsEmpty());

  // Test checking an origin and make sure the result is recorded to the
  // histogram.
  EXPECT_FALSE(CheckOriginIsPresent(GURL("https://example.com")));
}

TEST_F(MediaEngagementPreloadedListTest, LoadBadFormatFile) {
  ASSERT_FALSE(
      LoadFromFile(GetAbsolutePathToGeneratedTestFile(kBadFormatFilePath)));
  EXPECT_FALSE(IsLoaded());
  EXPECT_TRUE(IsEmpty());

  // Test checking an origin and make sure the result is recorded to the
  // histogram.
  EXPECT_FALSE(CheckOriginIsPresent(GURL("https://example.com")));
}

TEST_F(MediaEngagementPreloadedListTest, LoadEmptyFile) {
  ASSERT_TRUE(LoadFromFile(GetAbsolutePathToGeneratedTestFile(kEmptyFilePath)));
  EXPECT_TRUE(IsLoaded());
  EXPECT_TRUE(IsEmpty());
}

TEST_F(MediaEngagementPreloadedListTest, CheckOriginIsPresent_UnsecureSchemes) {
  ASSERT_TRUE(
      LoadFromFile(GetAbsolutePathToGeneratedTestFile(kSampleDataPath)));
  EXPECT_TRUE(IsLoaded());
  EXPECT_FALSE(IsEmpty());

  // An origin that has both HTTP and HTTPS entries should allow either.
  EXPECT_TRUE(CheckOriginIsPresent(GURL("https://google.com")));
  EXPECT_TRUE(CheckOriginIsPresent(GURL("http://google.com")));

  // An origin that only has a HTTP origin should allow either.
  EXPECT_TRUE(CheckOriginIsPresent(GURL("https://123.123.123.123")));
  EXPECT_TRUE(CheckOriginIsPresent(GURL("http://123.123.123.123")));

  // An origin that has only HTTPS should only allow HTTPS.
  EXPECT_TRUE(CheckOriginIsPresent(GURL("https://example.com")));
  EXPECT_FALSE(CheckOriginIsPresent(GURL("http://example.com")));
}
