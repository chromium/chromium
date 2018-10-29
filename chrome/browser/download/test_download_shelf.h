// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_TEST_DOWNLOAD_SHELF_H_
#define CHROME_BROWSER_DOWNLOAD_TEST_DOWNLOAD_SHELF_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/download/download_shelf.h"
#include "content/public/browser/download_manager.h"

// An implementation of DownloadShelf for testing.
class TestDownloadShelf : public DownloadShelf {
 public:
  TestDownloadShelf();
  ~TestDownloadShelf() override;

  // DownloadShelf:
  bool IsShowing() const override;
  bool IsClosing() const override;
  Browser* browser() const override;

  // Return |true| if a download was added to this shelf.
  bool did_add_download() const { return did_add_download_; }

  // Set a profile.
  void set_profile(Profile* profile) { profile_ = profile; }

 protected:
  void DoAddDownload(DownloadUIModelPtr download) override;
  void DoOpen() override;
  void DoClose(CloseReason reason) override;
  void DoHide() override;
  void DoUnhide() override;
  base::TimeDelta GetTransientDownloadShowDelay() override;
  Profile* profile() const override;

 private:
  bool is_showing_;
  bool did_add_download_;
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(TestDownloadShelf);
};

#endif  // CHROME_BROWSER_DOWNLOAD_TEST_DOWNLOAD_SHELF_H_
