// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_COMPLETION_BLOCKER_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_COMPLETION_BLOCKER_H_

#include "base/functional/callback.h"
#include "base/supports_user_data.h"
#include "components/download/public/common/download_item.h"

// A subsystem may use a DownloadCompletionBlocker in conjunction with
// DownloadManagerDelegate::ShouldCompleteDownload() in order to block the
// completion of a DownloadItem. CompleteDownload() will run the most recent
// callback set.
class DownloadCompletionBlocker : public base::SupportsUserData::Data {
 public:
  DownloadCompletionBlocker();

  DownloadCompletionBlocker(const DownloadCompletionBlocker&) = delete;
  DownloadCompletionBlocker& operator=(const DownloadCompletionBlocker&) =
      delete;

  ~DownloadCompletionBlocker() override;

  bool is_complete() const { return is_complete_; }

  void set_callback(base::OnceClosure callback) {
    if (!is_complete())
      callback_ = std::move(callback);
  }

  // Mark this download item as complete with respect to this blocker. (Other
  // blockers may continue to block the item.) Run |callback_|. This method may
  // only be called once, so |callback_| will only be called once.  Subclasses
  // must call the base implementation if they override this method.
  virtual void CompleteDownload();

 private:
  bool is_complete_;
  base::OnceClosure callback_;
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_COMPLETION_BLOCKER_H_
