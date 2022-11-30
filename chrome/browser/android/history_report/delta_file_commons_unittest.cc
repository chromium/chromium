// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/history_report/delta_file_commons.h"

#include <stdint.h>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/url_and_title.h"
#include "components/history/core/browser/history_types.h"
#include "testing/gtest/include/gtest/gtest.h"

using bookmarks::BookmarkModel;
using bookmarks::UrlAndTitle;

namespace history_report {

class DeltaFileEntryWithDataTest : public testing::Test {
 public:
  DeltaFileEntryWithDataTest()
      : entry_(),
        data_(entry_) {}

  DeltaFileEntryWithDataTest(const DeltaFileEntryWithDataTest&) = delete;
  DeltaFileEntryWithDataTest& operator=(const DeltaFileEntryWithDataTest&) =
      delete;

  ~DeltaFileEntryWithDataTest() override {}

 protected:
  void SetUp() override {}

  DeltaFileEntry entry_;
  DeltaFileEntryWithData data_;
};

TEST_F(DeltaFileEntryWithDataTest, NotValid) {
  EXPECT_FALSE(data_.Valid());
}

TEST_F(DeltaFileEntryWithDataTest, ValidDelEntry) {
  DeltaFileEntry entry;
  entry.set_type("del");
  DeltaFileEntryWithData data(entry);
  EXPECT_TRUE(data.Valid());
}

TEST_F(DeltaFileEntryWithDataTest, DelEntryWithData) {
  DeltaFileEntry entry;
  entry.set_type("del");
  DeltaFileEntryWithData data(entry);
  history::URLRow row;
  row.set_hidden(false);
  data.SetData(row);
  EXPECT_TRUE(data.Valid());
  // If deletion entry turns out to have data
  // then it's an update entry instead.
  EXPECT_EQ("add", data.Type());
}

TEST_F(DeltaFileEntryWithDataTest, Valid) {
  history::URLRow row;
  row.set_hidden(false);
  data_.SetData(row);
  EXPECT_TRUE(data_.Valid());
}

TEST_F(DeltaFileEntryWithDataTest, Hidden) {
  history::URLRow row;
  row.set_hidden(true);
  data_.SetData(row);
  EXPECT_FALSE(data_.Valid());
}

TEST_F(DeltaFileEntryWithDataTest, NoBookmarkScore) {
  history::URLRow row;
  row.set_hidden(false);
  row.set_typed_count(2);
  row.set_visit_count(3);
  data_.SetData(row);
  EXPECT_TRUE(data_.Valid());
  EXPECT_EQ(5, data_.Score());
}

TEST_F(DeltaFileEntryWithDataTest, BookmarkScore) {
  UrlAndTitle bookmark;
  history::URLRow row;
  row.set_hidden(false);
  row.set_typed_count(2);
  row.set_visit_count(3);
  data_.SetData(row);
  data_.MarkAsBookmark(bookmark);
  EXPECT_TRUE(data_.Valid());
  EXPECT_EQ(18, data_.Score());
}

TEST_F(DeltaFileEntryWithDataTest, NoBookmarkEmptyTitle) {
  history::URLRow row(GURL("http://www.host.org/path?query=param"));
  row.set_title(u"");
  row.set_hidden(false);
  data_.SetData(row);
  EXPECT_TRUE(data_.Valid());
  EXPECT_EQ(u"www.host.org", data_.Title());
}

TEST_F(DeltaFileEntryWithDataTest, NoBookmarkNonEmptyTitle) {
  history::URLRow row(GURL("http://host.org/path?query=param"));
  row.set_title(u"title");
  row.set_hidden(false);
  data_.SetData(row);
  EXPECT_TRUE(data_.Valid());
  EXPECT_EQ(u"title", data_.Title());
}

TEST_F(DeltaFileEntryWithDataTest, BookmarkTitle) {
  UrlAndTitle bookmark;
  bookmark.title = u"bookmark_title";
  history::URLRow row(GURL("http://host.org/path?query=param"));
  row.set_title(u"title");
  row.set_hidden(false);
  data_.SetData(row);
  data_.MarkAsBookmark(bookmark);
  EXPECT_TRUE(data_.Valid());
  EXPECT_EQ(u"bookmark_title", data_.Title());
}

TEST_F(DeltaFileEntryWithDataTest, TrimWWWPrefix) {
  history::URLRow row(GURL("http://www.host.org/path?query=param"));
  row.set_hidden(false);
  data_.SetData(row);
  EXPECT_TRUE(data_.Valid());
  EXPECT_EQ("host", data_.IndexedUrl());
}

TEST_F(DeltaFileEntryWithDataTest, TrimWW2Prefix) {
  history::URLRow row(GURL("http://ww2.host.org/path?query=param"));
  row.set_hidden(false);
  data_.SetData(row);
  EXPECT_TRUE(data_.Valid());
  EXPECT_EQ("host", data_.IndexedUrl());
}

TEST_F(DeltaFileEntryWithDataTest, TrimComSuffix) {
  history::URLRow row(GURL("http://host.com/path?query=param"));
  row.set_hidden(false);
  data_.SetData(row);
  EXPECT_TRUE(data_.Valid());
  EXPECT_EQ("host", data_.IndexedUrl());
}

TEST_F(DeltaFileEntryWithDataTest, TrimCoUKSuffix) {
  history::URLRow row(GURL("http://host.co.uk/path?query=param"));
  row.set_hidden(false);
  data_.SetData(row);
  EXPECT_TRUE(data_.Valid());
  EXPECT_EQ("host", data_.IndexedUrl());
}

TEST_F(DeltaFileEntryWithDataTest, TrimOrgKSuffix) {
  history::URLRow row(GURL("http://host.org/path?query=param"));
  row.set_hidden(false);
  data_.SetData(row);
  EXPECT_TRUE(data_.Valid());
  EXPECT_EQ("host", data_.IndexedUrl());
}

TEST_F(DeltaFileEntryWithDataTest, TrimRegionalSuffix) {
  history::URLRow row(GURL("http://host.waw.pl/path?query=param"));
  row.set_hidden(false);
  data_.SetData(row);
  EXPECT_TRUE(data_.Valid());
  EXPECT_EQ("host", data_.IndexedUrl());
}

TEST_F(DeltaFileEntryWithDataTest, TrimPrivateDomainSuffix) {
  history::URLRow row(GURL("http://host.appspot.com/path?query=param"));
  row.set_hidden(false);
  data_.SetData(row);
  EXPECT_TRUE(data_.Valid());
  EXPECT_EQ("host.appspot", data_.IndexedUrl());
}

TEST_F(DeltaFileEntryWithDataTest, IdForShortUrl) {
  std::string short_url("http://this.is.a.short.url.dot.com");

  EXPECT_TRUE(DeltaFileEntryWithData::IsValidId(short_url));

  DeltaFileEntry entry;
  entry.set_url(short_url);
  DeltaFileEntryWithData data(entry);

  EXPECT_EQ(short_url, data.Id());
}

TEST_F(DeltaFileEntryWithDataTest, IdForLongUrl) {

  std::stringstream url("http://domain.com/");

  while (DeltaFileEntryWithData::IsValidId(url.str())) {
    url << "a";
  }

  EXPECT_FALSE(DeltaFileEntryWithData::IsValidId(url.str()));

  DeltaFileEntry entry;
  entry.set_url(url.str());
  DeltaFileEntryWithData data(entry);

  EXPECT_NE(url.str(), data.Id());
  EXPECT_TRUE(DeltaFileEntryWithData::IsValidId(data.Id()))
      << data.Id() << " is not a valid ID";

  std::stringstream expected_length_stream;
  expected_length_stream << std::setfill('0') << std::setw(8)
                         << base::NumberToString(url.str().size());
  std::string length = data.Id().substr(0, 8);
  EXPECT_EQ(expected_length_stream.str(), length);

  std::string prefix = data.Id().substr(72);
  std::string expected_prefix(url.str().substr(0, prefix.size()));
  EXPECT_EQ(expected_prefix, prefix);
}

} // namespace history_report
