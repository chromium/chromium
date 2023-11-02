// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_TEST_DOWNLOAD_SHELF_H_
#define CHROME_BROWSER_DOWNLOAD_TEST_DOWNLOAD_SHELF_H_

#include "chrome/browser/download/download_shelf.h"

class Profile;

// An implementation of DownloadShelf for testing.
class TestDownloadShelf : public DownloadShelf {
 public:
  explicit TestDownloadShelf(Profile* profile);
  TestDownloadShelf(const TestDownloadShelf&) = delete;
  TestDownloadShelf& operator=(const TestDownloadShelf&) = delete;
  ~TestDownloadShelf() override;

  // DownloadShelf:
  bool IsShowing() const override;
  bool IsClosing() const override;

  views::View* GetView() override;

  bool did_add_download() const { return did_add_download_; }

 protected:
  void DoShowDownload(DownloadUIModel::DownloadUIModelPtr download) override;
  void DoOpen() override;
  void DoClose() override;
  void DoHide() override;
  void DoUnhide() override;
  base::TimeDelta GetTransientDownloadShowDelay() const override;

 private:
  bool is_showing_ = false;
  bool did_add_download_ = false;
};

#endif  // CHROME_BROWSER_DOWNLOAD_TEST_DOWNLOAD_SHELF_H_
