// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_DOWNLOAD_DOWNLOAD_UTILS_H_
#define CHROME_BROWSER_ANDROID_DOWNLOAD_DOWNLOAD_UTILS_H_

#include "base/files/file_path.h"

// Native side of DownloadUtils.java.
class DownloadUtils {
 public:
  static base::FilePath GetUriStringForPath(const base::FilePath& file_path);
};

#endif  // CHROME_BROWSER_ANDROID_DOWNLOAD_DOWNLOAD_UTILS_H_
