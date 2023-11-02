// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_completion_blocker.h"


DownloadCompletionBlocker::DownloadCompletionBlocker()
  : is_complete_(false) {
}

DownloadCompletionBlocker::~DownloadCompletionBlocker() {
}

void DownloadCompletionBlocker::CompleteDownload() {
  // Do not run |callback_| more than once.
  if (is_complete_)
    return;
  is_complete_ = true;

  if (callback_.is_null())
    return;
  std::move(callback_).Run();
  // |callback_| may delete |this|, so do not rely on |this| after running
  // |callback_|!
}
