// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_UKM_RECORDER_H_
#define CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_UKM_RECORDER_H_

#include <stdint.h>

#include "base/macros.h"

class GURL;

// WebApkUkmRecorder is the C++ counterpart of
// org.chromium.chrome.browser.webapps's WebApkUkmRecorder in Java.
// It contains static WebAPK UKM metrics-recording logic, and only
// needs to be in a class so that it can be a friend of ukm::UkmRecorder.
// All of the actual JNI goes through raw functions in webapk_ukm_recorder.cc to
// avoid having to instantiate this class and deal with object lifetimes.
class WebApkUkmRecorder {
 public:
  static void RecordInstall(const GURL& manifest_url, int version_code);

  static void RecordSessionDuration(const GURL& manifest_url,
                                    int64_t distributor,
                                    int64_t version_code,
                                    int64_t duration);

  static void RecordVisit(const GURL& manifest_url,
                          int64_t distributor,
                          int64_t version_code,
                          int source);

  static void RecordUninstall(const GURL& manifest_url,
                              int64_t distributor,
                              int64_t version_code,
                              int64_t launch_count,
                              int64_t install_duration);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(WebApkUkmRecorder);
};

#endif  // CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_UKM_RECORDER_H_
