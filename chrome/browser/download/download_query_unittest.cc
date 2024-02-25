// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_query.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/download/public/common/mock_download_item.h"
#include "content/public/browser/download_item_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using download::DownloadItem;
using ::testing::_;
using ::testing::Return;
using ::testing::ReturnRef;
using DownloadVector = DownloadQuery::DownloadVector;

namespace {

constexpr int kSomeKnownTime = 1355864160;
constexpr char kSomeKnownTime8601[] = "2012-12-18T20:56:0";
constexpr char k8601Suffix[] = ".000Z";

constexpr int64_t kEightGB = 1LL << 33;
constexpr int64_t kSixteenGB = 1LL << 34;
constexpr double kEightGBDouble = 8.0 * (1LL << 30);
constexpr double kNineGBDouble = 9.0 * (1LL << 30);

bool IdNotEqual(uint32_t not_id, const DownloadItem& item) {
  return item.GetId() != not_id;
}

bool AlwaysReturn(bool result, const DownloadItem& item) {
  return result;
}

}  // namespace

class DownloadQueryTest : public testing::Test {
 public:
  DownloadQueryTest() {}

  DownloadQueryTest(const DownloadQueryTest&) = delete;
  DownloadQueryTest& operator=(const DownloadQueryTest&) = delete;

  ~DownloadQueryTest() override {}

  void TearDown() override {}

  void CreateMocks(int count) {
    for (int i = 0; i < count; ++i) {
      owned_mocks_.push_back(std::make_unique<download::MockDownloadItem>());
      mocks_.push_back(owned_mocks_.back().get());
      EXPECT_CALL(mock(mocks_.size() - 1), GetId()).WillRepeatedly(Return(
          mocks_.size() - 1));
      content::DownloadItemUtils::AttachInfoForTesting(mocks_.back(), nullptr,
                                                       nullptr);
    }
  }

  download::MockDownloadItem& mock(int index) { return *mocks_[index]; }

  DownloadQuery* query() { return &query_; }

  template<typename ValueType> void AddFilter(
      DownloadQuery::FilterType name, ValueType value);

  void Search() {
    query_.Search(mocks_.begin(), mocks_.end(), &results_);
  }

  DownloadVector* results() { return &results_; }

  // Filter tests generally contain 2 items. mock(0) matches the filter, mock(1)
  // does not.
  void ExpectStandardFilterResults() {
    Search();
    ASSERT_EQ(1U, results()->size());
    ASSERT_EQ(0U, results()->at(0)->GetId());
  }

  // If no sorters distinguish between two items, then DownloadQuery sorts by ID
  // ascending. In order to test that a sorter distinguishes between two items,
  // the sorter must sort them by ID descending.
  void ExpectSortInverted() {
    Search();
    ASSERT_EQ(2U, results()->size());
    ASSERT_EQ(1U, results()->at(0)->GetId());
    ASSERT_EQ(0U, results()->at(1)->GetId());
  }

 private:
  // These two vectors hold the MockDownloadItems. |mocks_| contains just the
  // pointers, but is necessary because DownloadQuery processes vectors of
  // unowned pointers. |owned_mocks_| holds the ownership of the mock objects.
  std::vector<raw_ptr<download::MockDownloadItem, VectorExperimental>> mocks_;
  std::vector<std::unique_ptr<download::MockDownloadItem>> owned_mocks_;
  DownloadQuery query_;
  DownloadVector results_;
};

template<> void DownloadQueryTest::AddFilter(
    DownloadQuery::FilterType name, bool cpp_value) {
  CHECK(query_.AddFilter(name, base::Value(cpp_value)));
}

template <>
void DownloadQueryTest::AddFilter(DownloadQuery::FilterType name,
                                  double cpp_value) {
  CHECK(query_.AddFilter(name, base::Value(cpp_value)));
}

template<> void DownloadQueryTest::AddFilter(
    DownloadQuery::FilterType name, const char* cpp_value) {
  CHECK(query_.AddFilter(name, base::Value(cpp_value)));
}

template<> void DownloadQueryTest::AddFilter(
    DownloadQuery::FilterType name, std::string cpp_value) {
  CHECK(query_.AddFilter(name, base::Value(cpp_value)));
}

template <>
void DownloadQueryTest::AddFilter(DownloadQuery::FilterType name,
                                  const char16_t* cpp_value) {
  CHECK(query_.AddFilter(name, base::Value(cpp_value)));
}

template <>
void DownloadQueryTest::AddFilter(DownloadQuery::FilterType name,
                                  std::vector<std::u16string> cpp_value) {
  base::Value::List list;
  for (const auto& value : cpp_value)
    list.Append(value);
  CHECK(query_.AddFilter(name, base::Value(std::move(list))));
}

template<> void DownloadQueryTest::AddFilter(
    DownloadQuery::FilterType name, std::vector<std::string> cpp_value) {
  base::Value::List list;
  for (const auto& value : cpp_value)
    list.Append(std::move(value));
  CHECK(query_.AddFilter(name, base::Value(std::move(list))));
}

TEST_F(DownloadQueryTest, DownloadQueryTest_ZeroItems) {
  Search();
  EXPECT_EQ(0U, results()->size());
}

TEST_F(DownloadQueryTest, DownloadQueryTest_InvalidFilter) {
  base::Value value(0);
  EXPECT_FALSE(query()->AddFilter(static_cast<DownloadQuery::FilterType>(
                                      std::numeric_limits<int32_t>::max()),
                                  value));
}

TEST_F(DownloadQueryTest, DownloadQueryTest_EmptyQuery) {
  CreateMocks(2);
  Search();
  ASSERT_EQ(2U, results()->size());
  ASSERT_EQ(0U, results()->at(0)->GetId());
  ASSERT_EQ(1U, results()->at(1)->GetId());
}

TEST_F(DownloadQueryTest, DownloadQueryTest_Limit) {
  CreateMocks(2);
  query()->Limit(1);
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterGenericQueryFilename) {
  CreateMocks(2);
  base::FilePath match_filename(FILE_PATH_LITERAL("query"));
  EXPECT_CALL(mock(0), GetTargetFilePath()).WillRepeatedly(ReturnRef(
      match_filename));
  base::FilePath fail_filename(FILE_PATH_LITERAL("fail"));
  EXPECT_CALL(mock(1), GetTargetFilePath()).WillRepeatedly(ReturnRef(
      fail_filename));
  GURL fail_url("http://example.com/fail");
  EXPECT_CALL(mock(0), GetOriginalUrl()).WillRepeatedly(ReturnRef(fail_url));
  EXPECT_CALL(mock(1), GetOriginalUrl()).WillRepeatedly(ReturnRef(fail_url));
  EXPECT_CALL(mock(0), GetURL()).WillRepeatedly(ReturnRef(fail_url));
  EXPECT_CALL(mock(1), GetURL()).WillRepeatedly(ReturnRef(fail_url));
  std::vector<std::string> query_terms;
  query_terms.push_back("query");
  AddFilter(DownloadQuery::FILTER_QUERY, query_terms);
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterGenericQueryOriginalUrl) {
  CreateMocks(2);
  base::FilePath fail_filename(FILE_PATH_LITERAL("fail"));
  EXPECT_CALL(mock(0), GetTargetFilePath()).WillRepeatedly(ReturnRef(
      fail_filename));
  EXPECT_CALL(mock(1), GetTargetFilePath()).WillRepeatedly(ReturnRef(
      fail_filename));
  GURL match_url("http://query.com/query");
  EXPECT_CALL(mock(0), GetOriginalUrl()).WillRepeatedly(ReturnRef(match_url));
  GURL fail_url("http://example.com/fail");
  EXPECT_CALL(mock(1), GetOriginalUrl()).WillRepeatedly(ReturnRef(fail_url));
  EXPECT_CALL(mock(0), GetURL()).WillRepeatedly(ReturnRef(fail_url));
  EXPECT_CALL(mock(1), GetURL()).WillRepeatedly(ReturnRef(fail_url));
  std::vector<std::string> query_terms;
  query_terms.push_back("query");
  AddFilter(DownloadQuery::FILTER_QUERY, query_terms);
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest,
       DownloadQueryTest_FilterGenericQueryOriginalUrlUnescaping) {
  CreateMocks(2);
  base::FilePath fail_filename(FILE_PATH_LITERAL("fail"));
  EXPECT_CALL(mock(0), GetTargetFilePath()).WillRepeatedly(ReturnRef(
      fail_filename));
  EXPECT_CALL(mock(1), GetTargetFilePath()).WillRepeatedly(ReturnRef(
      fail_filename));
  GURL match_url("http://q%75%65%72y.c%6Fm/%71uer%79");
  GURL fail_url("http://%65xampl%65.com/%66ai%6C");
  EXPECT_CALL(mock(0), GetOriginalUrl()).WillRepeatedly(ReturnRef(match_url));
  EXPECT_CALL(mock(1), GetOriginalUrl()).WillRepeatedly(ReturnRef(fail_url));
  EXPECT_CALL(mock(0), GetURL()).WillRepeatedly(ReturnRef(fail_url));
  EXPECT_CALL(mock(1), GetURL()).WillRepeatedly(ReturnRef(fail_url));
  std::vector<std::string> query_terms;
  query_terms.push_back("query");
  AddFilter(DownloadQuery::FILTER_QUERY, query_terms);
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterGenericQueryUrl) {
  CreateMocks(2);
  base::FilePath fail_filename(FILE_PATH_LITERAL("fail"));
  EXPECT_CALL(mock(0), GetTargetFilePath()).WillRepeatedly(ReturnRef(
      fail_filename));
  EXPECT_CALL(mock(1), GetTargetFilePath()).WillRepeatedly(ReturnRef(
      fail_filename));
  GURL match_url("http://query.com/query");
  GURL fail_url("http://example.com/fail");
  EXPECT_CALL(mock(0), GetOriginalUrl()).WillRepeatedly(ReturnRef(fail_url));
  EXPECT_CALL(mock(1), GetOriginalUrl()).WillRepeatedly(ReturnRef(fail_url));
  EXPECT_CALL(mock(0), GetURL()).WillRepeatedly(ReturnRef(match_url));
  EXPECT_CALL(mock(1), GetURL()).WillRepeatedly(ReturnRef(fail_url));
  std::vector<std::string> query_terms;
  query_terms.push_back("query");
  AddFilter(DownloadQuery::FILTER_QUERY, query_terms);
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterGenericQueryUrlUnescaping) {
  CreateMocks(2);
  base::FilePath fail_filename(FILE_PATH_LITERAL("fail"));
  EXPECT_CALL(mock(0), GetTargetFilePath()).WillRepeatedly(ReturnRef(
      fail_filename));
  EXPECT_CALL(mock(1), GetTargetFilePath()).WillRepeatedly(ReturnRef(
      fail_filename));
  GURL match_url("http://%71uer%79.com/qu%65ry");
  GURL fail_url("http://e%78am%70le.com/f%61il");
  EXPECT_CALL(mock(0), GetOriginalUrl()).WillRepeatedly(ReturnRef(fail_url));
  EXPECT_CALL(mock(1), GetOriginalUrl()).WillRepeatedly(ReturnRef(fail_url));
  EXPECT_CALL(mock(0), GetURL()).WillRepeatedly(ReturnRef(match_url));
  EXPECT_CALL(mock(1), GetURL()).WillRepeatedly(ReturnRef(fail_url));
  std::vector<std::string> query_terms;
  query_terms.push_back("query");
  AddFilter(DownloadQuery::FILTER_QUERY, query_terms);
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterGenericQueryFilenameI18N) {
  CreateMocks(2);
  const std::string kTestString(
      "/\xe4\xbd\xa0\xe5\xa5\xbd\xe4\xbd\xa0\xe5\xa5\xbd");
  base::FilePath match_filename = base::FilePath::FromUTF8Unsafe(kTestString);
  EXPECT_CALL(mock(0), GetTargetFilePath()).WillRepeatedly(ReturnRef(
      match_filename));
  base::FilePath fail_filename(FILE_PATH_LITERAL("fail"));
  EXPECT_CALL(mock(1), GetTargetFilePath()).WillRepeatedly(ReturnRef(
      fail_filename));
  GURL fail_url("http://example.com/fail");
  EXPECT_CALL(mock(0), GetOriginalUrl()).WillRepeatedly(ReturnRef(fail_url));
  EXPECT_CALL(mock(1), GetOriginalUrl()).WillRepeatedly(ReturnRef(fail_url));
  EXPECT_CALL(mock(0), GetURL()).WillRepeatedly(ReturnRef(fail_url));
  EXPECT_CALL(mock(1), GetURL()).WillRepeatedly(ReturnRef(fail_url));
  std::vector<std::string> query_terms;
  query_terms.push_back(kTestString);
  AddFilter(DownloadQuery::FILTER_QUERY, query_terms);
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterFilenameRegex) {
  CreateMocks(2);
  base::FilePath match_filename(FILE_PATH_LITERAL("query"));
  EXPECT_CALL(mock(0), GetTargetFilePath()).WillRepeatedly(ReturnRef(
      match_filename));
  base::FilePath fail_filename(FILE_PATH_LITERAL("fail"));
  EXPECT_CALL(mock(1), GetTargetFilePath()).WillRepeatedly(ReturnRef(
      fail_filename));
  AddFilter(DownloadQuery::FILTER_FILENAME_REGEX, "y");
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_SortFilename) {
  CreateMocks(2);
  base::FilePath b_filename(FILE_PATH_LITERAL("b"));
  EXPECT_CALL(mock(0), GetTargetFilePath()).WillRepeatedly(ReturnRef(
      b_filename));
  base::FilePath a_filename(FILE_PATH_LITERAL("a"));
  EXPECT_CALL(mock(1), GetTargetFilePath()).WillRepeatedly(ReturnRef(
      a_filename));
  query()->AddSorter(DownloadQuery::SORT_FILENAME, DownloadQuery::ASCENDING);
  ExpectSortInverted();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterFilename) {
  CreateMocks(2);
  base::FilePath match_filename(FILE_PATH_LITERAL("query"));
  EXPECT_CALL(mock(0), GetTargetFilePath()).WillRepeatedly(ReturnRef(
      match_filename));
  base::FilePath fail_filename(FILE_PATH_LITERAL("fail"));
  EXPECT_CALL(mock(1), GetTargetFilePath()).WillRepeatedly(ReturnRef(
      fail_filename));
  AddFilter(DownloadQuery::FILTER_FILENAME,
            match_filename.AsUTF8Unsafe().c_str());
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterOriginalUrlRegex) {
  CreateMocks(2);
  GURL match_url("http://query.com/query");
  EXPECT_CALL(mock(0), GetOriginalUrl()).WillRepeatedly(ReturnRef(match_url));
  GURL fail_url("http://example.com/fail");
  EXPECT_CALL(mock(1), GetOriginalUrl()).WillRepeatedly(ReturnRef(fail_url));
  AddFilter(DownloadQuery::FILTER_ORIGINAL_URL_REGEX, "query");
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_SortOriginalUrl) {
  CreateMocks(2);
  GURL b_url("http://example.com/b");
  EXPECT_CALL(mock(0), GetOriginalUrl()).WillRepeatedly(ReturnRef(b_url));
  GURL a_url("http://example.com/a");
  EXPECT_CALL(mock(1), GetOriginalUrl()).WillRepeatedly(ReturnRef(a_url));
  query()->AddSorter(
      DownloadQuery::SORT_ORIGINAL_URL, DownloadQuery::ASCENDING);
  ExpectSortInverted();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterOriginalUrl) {
  CreateMocks(2);
  GURL match_url("http://query.com/query");
  EXPECT_CALL(mock(0), GetOriginalUrl()).WillRepeatedly(ReturnRef(match_url));
  GURL fail_url("http://example.com/fail");
  EXPECT_CALL(mock(1), GetOriginalUrl()).WillRepeatedly(ReturnRef(fail_url));
  AddFilter(DownloadQuery::FILTER_ORIGINAL_URL, match_url.spec().c_str());
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterUrlRegex) {
  CreateMocks(2);
  GURL match_url("http://query.com/query");
  EXPECT_CALL(mock(0), GetURL()).WillRepeatedly(ReturnRef(match_url));
  GURL fail_url("http://example.com/fail");
  EXPECT_CALL(mock(1), GetURL()).WillRepeatedly(ReturnRef(fail_url));
  AddFilter(DownloadQuery::FILTER_URL_REGEX, "query");
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_SortUrl) {
  CreateMocks(2);
  GURL b_url("http://example.com/b");
  EXPECT_CALL(mock(0), GetURL()).WillRepeatedly(ReturnRef(b_url));
  GURL a_url("http://example.com/a");
  EXPECT_CALL(mock(1), GetURL()).WillRepeatedly(ReturnRef(a_url));
  query()->AddSorter(DownloadQuery::SORT_URL, DownloadQuery::ASCENDING);
  ExpectSortInverted();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterUrl) {
  CreateMocks(2);
  GURL match_url("http://query.com/query");
  EXPECT_CALL(mock(0), GetURL()).WillRepeatedly(ReturnRef(match_url));
  GURL fail_url("http://example.com/fail");
  EXPECT_CALL(mock(1), GetURL()).WillRepeatedly(ReturnRef(fail_url));
  AddFilter(DownloadQuery::FILTER_URL, match_url.spec().c_str());
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterCallback) {
  CreateMocks(2);
  CHECK(query()->AddFilter(base::BindRepeating(&IdNotEqual, 1)));
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterBytesReceived) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetReceivedBytes()).WillRepeatedly(Return(0));
  EXPECT_CALL(mock(1), GetReceivedBytes()).WillRepeatedly(Return(1));
  AddFilter(DownloadQuery::FILTER_BYTES_RECEIVED, 0.0);
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_SortBytesReceived) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetReceivedBytes()).WillRepeatedly(Return(0));
  EXPECT_CALL(mock(1), GetReceivedBytes()).WillRepeatedly(Return(1));
  query()->AddSorter(DownloadQuery::SORT_BYTES_RECEIVED,
                     DownloadQuery::DESCENDING);
  ExpectSortInverted();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterDangerAccepted) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetDangerType())
      .WillRepeatedly(Return(download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED));
  EXPECT_CALL(mock(1), GetDangerType())
      .WillRepeatedly(Return(download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE));
  AddFilter(DownloadQuery::FILTER_DANGER_ACCEPTED, true);
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_SortDangerAccepted) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetDangerType())
      .WillRepeatedly(Return(download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED));
  EXPECT_CALL(mock(1), GetDangerType())
      .WillRepeatedly(Return(download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE));
  query()->AddSorter(DownloadQuery::SORT_DANGER_ACCEPTED,
                     DownloadQuery::ASCENDING);
  ExpectSortInverted();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterExists) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetFileExternallyRemoved()).WillRepeatedly(Return(
      false));
  EXPECT_CALL(mock(1), GetFileExternallyRemoved()).WillRepeatedly(Return(
      true));
  AddFilter(DownloadQuery::FILTER_EXISTS, true);
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_SortExists) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetFileExternallyRemoved()).WillRepeatedly(Return(
      false));
  EXPECT_CALL(mock(1), GetFileExternallyRemoved()).WillRepeatedly(Return(
      true));
  query()->AddSorter(DownloadQuery::SORT_EXISTS,
                     DownloadQuery::ASCENDING);
  ExpectSortInverted();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterMime) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetMimeType()).WillRepeatedly(Return("text"));
  EXPECT_CALL(mock(1), GetMimeType()).WillRepeatedly(Return("image"));
  AddFilter(DownloadQuery::FILTER_MIME, "text");
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_SortMime) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetMimeType()).WillRepeatedly(Return("b"));
  EXPECT_CALL(mock(1), GetMimeType()).WillRepeatedly(Return("a"));
  query()->AddSorter(DownloadQuery::SORT_MIME, DownloadQuery::ASCENDING);
  ExpectSortInverted();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterPaused) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), IsPaused()).WillRepeatedly(Return(true));
  EXPECT_CALL(mock(1), IsPaused()).WillRepeatedly(Return(false));
  AddFilter(DownloadQuery::FILTER_PAUSED, true);
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_SortPaused) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), IsPaused()).WillRepeatedly(Return(true));
  EXPECT_CALL(mock(1), IsPaused()).WillRepeatedly(Return(false));
  query()->AddSorter(DownloadQuery::SORT_PAUSED, DownloadQuery::ASCENDING);
  ExpectSortInverted();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterStartedAfter) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetStartTime()).WillRepeatedly(Return(
      base::Time::FromTimeT(kSomeKnownTime + 2)));
  EXPECT_CALL(mock(1), GetStartTime()).WillRepeatedly(Return(
      base::Time::FromTimeT(kSomeKnownTime + 1)));
  AddFilter(DownloadQuery::FILTER_STARTED_AFTER,
            std::string(kSomeKnownTime8601) + "1" + std::string(k8601Suffix));
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterStartedBefore) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetStartTime()).WillRepeatedly(Return(
      base::Time::FromTimeT(kSomeKnownTime + 2)));
  EXPECT_CALL(mock(1), GetStartTime()).WillRepeatedly(Return(
      base::Time::FromTimeT(kSomeKnownTime + 4)));
  AddFilter(DownloadQuery::FILTER_STARTED_BEFORE,
            std::string(kSomeKnownTime8601) + "4" + std::string(k8601Suffix));
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterStartTime) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetStartTime()).WillRepeatedly(Return(
      base::Time::FromTimeT(kSomeKnownTime + 2)));
  EXPECT_CALL(mock(1), GetStartTime()).WillRepeatedly(Return(
      base::Time::FromTimeT(kSomeKnownTime + 4)));
  AddFilter(DownloadQuery::FILTER_START_TIME,
            std::string(kSomeKnownTime8601) + "2" + std::string(k8601Suffix));
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_SortStartTime) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetStartTime()).WillRepeatedly(Return(
      base::Time::FromTimeT(kSomeKnownTime + 2)));
  EXPECT_CALL(mock(1), GetStartTime()).WillRepeatedly(Return(
      base::Time::FromTimeT(kSomeKnownTime + 4)));
  query()->AddSorter(DownloadQuery::SORT_START_TIME, DownloadQuery::DESCENDING);
  ExpectSortInverted();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterEndedAfter) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetEndTime()).WillRepeatedly(Return(
      base::Time::FromTimeT(kSomeKnownTime + 2)));
  EXPECT_CALL(mock(1), GetEndTime()).WillRepeatedly(Return(
      base::Time::FromTimeT(kSomeKnownTime + 1)));
  AddFilter(DownloadQuery::FILTER_ENDED_AFTER,
            std::string(kSomeKnownTime8601) + "1" + std::string(k8601Suffix));
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterEndedBefore) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetEndTime()).WillRepeatedly(Return(
      base::Time::FromTimeT(kSomeKnownTime + 2)));
  EXPECT_CALL(mock(1), GetEndTime()).WillRepeatedly(Return(
      base::Time::FromTimeT(kSomeKnownTime + 4)));
  AddFilter(DownloadQuery::FILTER_ENDED_BEFORE,
            std::string(kSomeKnownTime8601) + "4" + std::string(k8601Suffix));
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterEndTime) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetEndTime()).WillRepeatedly(Return(
      base::Time::FromTimeT(kSomeKnownTime + 2)));
  EXPECT_CALL(mock(1), GetEndTime()).WillRepeatedly(Return(
      base::Time::FromTimeT(kSomeKnownTime + 4)));
  AddFilter(DownloadQuery::FILTER_END_TIME,
            std::string(kSomeKnownTime8601) + "2" + std::string(k8601Suffix));
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_SortEndTime) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetEndTime()).WillRepeatedly(Return(
      base::Time::FromTimeT(kSomeKnownTime + 2)));
  EXPECT_CALL(mock(1), GetEndTime()).WillRepeatedly(Return(
      base::Time::FromTimeT(kSomeKnownTime + 4)));
  query()->AddSorter(DownloadQuery::SORT_END_TIME, DownloadQuery::DESCENDING);
  ExpectSortInverted();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterTotalBytesGreater1) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetTotalBytes()).WillRepeatedly(Return(2));
  EXPECT_CALL(mock(1), GetTotalBytes()).WillRepeatedly(Return(1));
  AddFilter(DownloadQuery::FILTER_TOTAL_BYTES_GREATER, 1.0);
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterTotalBytesGreater2) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetTotalBytes()).WillRepeatedly(Return(2));
  EXPECT_CALL(mock(1), GetTotalBytes()).WillRepeatedly(Return(1));
  AddFilter(DownloadQuery::FILTER_TOTAL_BYTES_GREATER, 1.2);
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterTotalBytesGreater3) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetTotalBytes()).WillRepeatedly(Return(kSixteenGB));
  EXPECT_CALL(mock(1), GetTotalBytes()).WillRepeatedly(Return(kEightGB));
  AddFilter(DownloadQuery::FILTER_TOTAL_BYTES_GREATER, kNineGBDouble);
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterTotalBytesGreater4) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetTotalBytes()).WillRepeatedly(Return(kSixteenGB));
  EXPECT_CALL(mock(1), GetTotalBytes()).WillRepeatedly(Return(kEightGB));
  AddFilter(DownloadQuery::FILTER_TOTAL_BYTES_GREATER, kEightGBDouble + 1.0);
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterTotalBytesLess1) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetTotalBytes()).WillRepeatedly(Return(2));
  EXPECT_CALL(mock(1), GetTotalBytes()).WillRepeatedly(Return(4));
  AddFilter(DownloadQuery::FILTER_TOTAL_BYTES_LESS, 4.0);
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterTotalBytesLess2) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetTotalBytes()).WillRepeatedly(Return(1));
  EXPECT_CALL(mock(1), GetTotalBytes()).WillRepeatedly(Return(2));
  AddFilter(DownloadQuery::FILTER_TOTAL_BYTES_LESS, 1.2);
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterTotalBytesLess3) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetTotalBytes()).WillRepeatedly(Return(kEightGB));
  EXPECT_CALL(mock(1), GetTotalBytes()).WillRepeatedly(Return(kSixteenGB));
  AddFilter(DownloadQuery::FILTER_TOTAL_BYTES_LESS, kEightGBDouble + 1.0);
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterTotalBytesLess4) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetTotalBytes()).WillRepeatedly(Return(kEightGB));
  EXPECT_CALL(mock(1), GetTotalBytes()).WillRepeatedly(Return(kSixteenGB));
  AddFilter(DownloadQuery::FILTER_TOTAL_BYTES_LESS, kNineGBDouble);
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterTotalBytes1) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetTotalBytes()).WillRepeatedly(Return(2));
  EXPECT_CALL(mock(1), GetTotalBytes()).WillRepeatedly(Return(4));
  AddFilter(DownloadQuery::FILTER_TOTAL_BYTES, 2.0);
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterTotalBytes2) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetTotalBytes()).WillRepeatedly(Return(1));
  EXPECT_CALL(mock(1), GetTotalBytes()).WillRepeatedly(Return(2));
  AddFilter(DownloadQuery::FILTER_TOTAL_BYTES, 1.0);
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterTotalBytes3) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetTotalBytes()).WillRepeatedly(Return(kEightGB));
  EXPECT_CALL(mock(1), GetTotalBytes()).WillRepeatedly(Return(kSixteenGB));
  AddFilter(DownloadQuery::FILTER_TOTAL_BYTES, kEightGBDouble);
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_SortTotalBytes) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetTotalBytes()).WillRepeatedly(Return(2));
  EXPECT_CALL(mock(1), GetTotalBytes()).WillRepeatedly(Return(4));
  query()->AddSorter(DownloadQuery::SORT_TOTAL_BYTES,
                     DownloadQuery::DESCENDING);
  ExpectSortInverted();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterState) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetState()).WillRepeatedly(Return(
      DownloadItem::IN_PROGRESS));
  EXPECT_CALL(mock(1), GetState()).WillRepeatedly(Return(
      DownloadItem::CANCELLED));
  query()->AddFilter(DownloadItem::IN_PROGRESS);
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_SortState) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetState()).WillRepeatedly(Return(
      DownloadItem::IN_PROGRESS));
  EXPECT_CALL(mock(1), GetState()).WillRepeatedly(Return(
      DownloadItem::CANCELLED));
  query()->AddSorter(DownloadQuery::SORT_STATE, DownloadQuery::DESCENDING);
  ExpectSortInverted();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_FilterDanger) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetDangerType())
      .WillRepeatedly(Return(download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS));
  EXPECT_CALL(mock(1), GetDangerType())
      .WillRepeatedly(Return(download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE));
  query()->AddFilter(download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);
  ExpectStandardFilterResults();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_SortDanger) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetDangerType())
      .WillRepeatedly(Return(download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS));
  EXPECT_CALL(mock(1), GetDangerType())
      .WillRepeatedly(Return(download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE));
  query()->AddSorter(DownloadQuery::SORT_DANGER, DownloadQuery::DESCENDING);
  ExpectSortInverted();
}

TEST_F(DownloadQueryTest, DownloadQueryTest_DefaultSortById1) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetReceivedBytes()).WillRepeatedly(Return(0));
  EXPECT_CALL(mock(1), GetReceivedBytes()).WillRepeatedly(Return(0));
  query()->AddSorter(DownloadQuery::SORT_BYTES_RECEIVED,
                     DownloadQuery::ASCENDING);
  Search();
  ASSERT_EQ(2U, results()->size());
  EXPECT_EQ(0U, results()->at(0)->GetId());
  EXPECT_EQ(1U, results()->at(1)->GetId());
}

TEST_F(DownloadQueryTest, DownloadQueryTest_DefaultSortById2) {
  CreateMocks(2);
  EXPECT_CALL(mock(0), GetReceivedBytes()).WillRepeatedly(Return(0));
  EXPECT_CALL(mock(1), GetReceivedBytes()).WillRepeatedly(Return(0));
  query()->AddSorter(DownloadQuery::SORT_BYTES_RECEIVED,
                     DownloadQuery::DESCENDING);
  Search();
  ASSERT_EQ(2U, results()->size());
  EXPECT_EQ(0U, results()->at(0)->GetId());
  EXPECT_EQ(1U, results()->at(1)->GetId());
}

TEST_F(DownloadQueryTest, DownloadQueryFilterPerformance) {
  static const int kNumItems = 100;
  static const int kNumFilters = 100;
  CreateMocks(kNumItems);
  for (size_t i = 0; i < (kNumFilters - 1); ++i) {
    query()->AddFilter(base::BindRepeating(&AlwaysReturn, true));
  }
  query()->AddFilter(base::BindRepeating(&AlwaysReturn, false));
  base::Time start = base::Time::Now();
  Search();
  base::Time end = base::Time::Now();
  double nanos = (end - start).InMillisecondsF() * 1000.0 * 1000.0;
  double nanos_per_item = nanos / static_cast<double>(kNumItems);
  double nanos_per_item_per_filter = nanos_per_item
    / static_cast<double>(kNumFilters);
  std::cout << "Search took " << nanos_per_item_per_filter
            << " nanoseconds per item per filter.\n";
}
