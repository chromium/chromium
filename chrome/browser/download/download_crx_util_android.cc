// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Download code which handles CRX files (extensions, themes, apps, ...).

#include "chrome/browser/download/download_crx_util.h"

namespace download_crx_util {

bool IsExtensionDownload(const download::DownloadItem& download_item) {
  // Extensions are not supported on Android. We want to treat them as
  // normal file downloads.
  return false;
}

bool IsTrustedExtensionDownload(Profile* profile,
                                const download::DownloadItem& item) {
  // Extensions are not supported on Android, return the safe default.
  return false;
}

}  // namespace download_crx_util
