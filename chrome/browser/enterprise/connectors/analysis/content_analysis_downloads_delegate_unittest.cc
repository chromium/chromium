// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/content_analysis_downloads_delegate.h"

#include <gtest/gtest.h>

#include "components/download/public/common/mock_download_item.h"

namespace enterprise_connectors {

class ContentAnalysisDownloadsDelegateTest : public testing::Test {
 public:
  void OpenCallback() { ++times_open_called_; }

  void DiscardCallback() { ++times_discard_called_; }

  int times_open_called_ = 0;
  int times_discard_called_ = 0;
  download::MockDownloadItem mock_download_item;
};

TEST_F(ContentAnalysisDownloadsDelegateTest, TestOpenFile) {
  ContentAnalysisDownloadsDelegate delegate(
      u"", u"", GURL(), true,
      base::BindOnce(&ContentAnalysisDownloadsDelegateTest::OpenCallback,
                     base::Unretained(this)),
      base::BindOnce(&ContentAnalysisDownloadsDelegateTest::DiscardCallback,
                     base::Unretained(this)),
      &mock_download_item);

  delegate.BypassWarnings(u"User's justification");
  EXPECT_EQ(1, times_open_called_);
  EXPECT_EQ(0, times_discard_called_);

  // Attempting any action after one has been performed is a no-op.
  delegate.BypassWarnings(u"User's justification");
  EXPECT_EQ(1, times_open_called_);
  EXPECT_EQ(0, times_discard_called_);

  delegate.Cancel(true);
  EXPECT_EQ(1, times_open_called_);
  EXPECT_EQ(0, times_discard_called_);

  delegate.Cancel(false);
  EXPECT_EQ(1, times_open_called_);
  EXPECT_EQ(0, times_discard_called_);
}

TEST_F(ContentAnalysisDownloadsDelegateTest, TestDiscardFileWarning) {
  ContentAnalysisDownloadsDelegate delegate(
      u"", u"", GURL(), true,
      base::BindOnce(&ContentAnalysisDownloadsDelegateTest::OpenCallback,
                     base::Unretained(this)),
      base::BindOnce(&ContentAnalysisDownloadsDelegateTest::DiscardCallback,
                     base::Unretained(this)),
      &mock_download_item);

  delegate.Cancel(true);
  EXPECT_EQ(0, times_open_called_);
  EXPECT_EQ(1, times_discard_called_);

  // Attempting any action after one has been performed is a no-op.
  delegate.Cancel(true);
  EXPECT_EQ(0, times_open_called_);
  EXPECT_EQ(1, times_discard_called_);

  delegate.Cancel(false);
  EXPECT_EQ(0, times_open_called_);
  EXPECT_EQ(1, times_discard_called_);

  delegate.BypassWarnings(absl::nullopt);
  EXPECT_EQ(0, times_open_called_);
  EXPECT_EQ(1, times_discard_called_);
}

TEST_F(ContentAnalysisDownloadsDelegateTest, TestDiscardFileBlock) {
  ContentAnalysisDownloadsDelegate delegate(
      u"", u"", GURL(), true,
      base::BindOnce(&ContentAnalysisDownloadsDelegateTest::OpenCallback,
                     base::Unretained(this)),
      base::BindOnce(&ContentAnalysisDownloadsDelegateTest::DiscardCallback,
                     base::Unretained(this)),
      &mock_download_item);

  delegate.Cancel(false);
  EXPECT_EQ(0, times_open_called_);
  EXPECT_EQ(1, times_discard_called_);

  // Attempting any action after one has been performed is a no-op.
  delegate.Cancel(true);
  EXPECT_EQ(0, times_open_called_);
  EXPECT_EQ(1, times_discard_called_);

  delegate.Cancel(false);
  EXPECT_EQ(0, times_open_called_);
  EXPECT_EQ(1, times_discard_called_);

  delegate.BypassWarnings(absl::nullopt);
  EXPECT_EQ(0, times_open_called_);
  EXPECT_EQ(1, times_discard_called_);
}

TEST_F(ContentAnalysisDownloadsDelegateTest, TestNoMessageOrUrlReturnsNullOpt) {
  ContentAnalysisDownloadsDelegate delegate(
      u"", u"", GURL(), true,
      base::BindOnce(&ContentAnalysisDownloadsDelegateTest::OpenCallback,
                     base::Unretained(this)),
      base::BindOnce(&ContentAnalysisDownloadsDelegateTest::DiscardCallback,
                     base::Unretained(this)),
      &mock_download_item);

  EXPECT_FALSE(delegate.GetCustomMessage());
  EXPECT_FALSE(delegate.GetCustomLearnMoreUrl());
}

TEST_F(ContentAnalysisDownloadsDelegateTest, TestGetMessageAndUrl) {
  ContentAnalysisDownloadsDelegate delegate(
      u"foo.txt", u"Message", GURL("http://www.example.com"), true,
      base::BindOnce(&ContentAnalysisDownloadsDelegateTest::OpenCallback,
                     base::Unretained(this)),
      base::BindOnce(&ContentAnalysisDownloadsDelegateTest::DiscardCallback,
                     base::Unretained(this)),
      nullptr);

  EXPECT_TRUE(delegate.GetCustomMessage());
  EXPECT_TRUE(delegate.GetCustomLearnMoreUrl());

  EXPECT_EQ(
      u"foo.txt has sensitive or dangerous data. Your administrator says: "
      u"\"Message\"",
      *(delegate.GetCustomMessage()));
  EXPECT_EQ(GURL("http://www.example.com"),
            *(delegate.GetCustomLearnMoreUrl()));
}

}  // namespace enterprise_connectors
