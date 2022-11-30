// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_status_updater.h"

#include "base/notreached.h"

void DownloadStatusUpdater::UpdateAppIconDownloadProgress(
    download::DownloadItem* download) {
  // TODO(crbug.com/1226242): Integrate with shell app-icon updates, if/when
  // the platform supports them.
  NOTIMPLEMENTED_LOG_ONCE();
}
