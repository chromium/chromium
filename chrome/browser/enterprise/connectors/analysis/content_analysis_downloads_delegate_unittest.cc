// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/content_analysis_downloads_delegate.h"

#include <gtest/gtest.h>

namespace enterprise_connectors {

class ContentAnalysisDownloadsDelegateTest : public testing::Test {
 public:
  void OpenCallback() { ++times_open_called_; }

  void DiscardCallback() { ++times_discard_called_; }

  int times_open_called_ = 0;
  int times_discard_called_ = 0;
};

TEST_F(ContentAnalysisDownloadsDelegateTest, TestOpenFile) {
  ContentAnalysisDownloadsDelegate delegate(
      base::BindOnce(&ContentAnalysisDownloadsDelegateTest::OpenCallback,
                     base::Unretained(this)),
      base::BindOnce(&ContentAnalysisDownloadsDelegateTest::DiscardCallback,
                     base::Unretained(this)));

  delegate.BypassWarnings();
  EXPECT_EQ(1, times_open_called_);
  EXPECT_EQ(0, times_discard_called_);

  // Attempting any action after one has been performed is a no-op.
  delegate.BypassWarnings();
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
      base::BindOnce(&ContentAnalysisDownloadsDelegateTest::OpenCallback,
                     base::Unretained(this)),
      base::BindOnce(&ContentAnalysisDownloadsDelegateTest::DiscardCallback,
                     base::Unretained(this)));

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

  delegate.BypassWarnings();
  EXPECT_EQ(0, times_open_called_);
  EXPECT_EQ(1, times_discard_called_);
}

TEST_F(ContentAnalysisDownloadsDelegateTest, TestDiscardFileBlock) {
  ContentAnalysisDownloadsDelegate delegate(
      base::BindOnce(&ContentAnalysisDownloadsDelegateTest::OpenCallback,
                     base::Unretained(this)),
      base::BindOnce(&ContentAnalysisDownloadsDelegateTest::DiscardCallback,
                     base::Unretained(this)));

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

  delegate.BypassWarnings();
  EXPECT_EQ(0, times_open_called_);
  EXPECT_EQ(1, times_discard_called_);
}

}  // namespace enterprise_connectors
