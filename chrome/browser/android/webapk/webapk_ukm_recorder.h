// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_UKM_RECORDER_H_
#define CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_UKM_RECORDER_H_

#include <stdint.h>

#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"

class GURL;

namespace webapps {
enum class WebappInstallSource;
}

namespace webapk {

// WebApkUkmRecorder is the C++ counterpart of
// org.chromium.chrome.browser.webapps's WebApkUkmRecorder in Java.
// It contains static WebAPK UKM metrics-recording logic, and only
// needs to be in a class so that it can be a friend of ukm::UkmRecorder.
// All of the actual JNI goes through raw functions in webapk_ukm_recorder.cc to
// avoid having to instantiate this class and deal with object lifetimes.
class WebApkUkmRecorder {
 public:
  WebApkUkmRecorder() = delete;
  WebApkUkmRecorder(const WebApkUkmRecorder&) = delete;
  WebApkUkmRecorder& operator=(const WebApkUkmRecorder&) = delete;

  static void RecordInstall(const GURL& manifest_id,
                            webapps::WebappInstallSource install_source,
                            blink::mojom::DisplayMode display);

  static void RecordSessionDuration(const GURL& manifest_id,
                                    int64_t distributor,
                                    int64_t version_code,
                                    int64_t duration);

  static void RecordVisit(const GURL& manifest_id,
                          int64_t distributor,
                          int64_t version_code,
                          int source);

  static void RecordUninstall(const GURL& manifest_id,
                              int64_t distributor,
                              int64_t version_code,
                              int64_t launch_count,
                              int64_t install_duration);

  // RecordWebApkableVisit records a visit to an installable PWA from a
  // non-installed surface on Android (ie, if an installable site is visited
  // from within a regular browser tab).
  //
  // Note that the metric will be recorded whether or not the PWA is actually
  // installed - all that matters is that it is being visited from a
  // "non-installed experience" (ie, as a normal browser tab).
  static void RecordWebApkableVisit(const GURL& manifest_id);
};
}  // namespace webapk

#endif  // CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_UKM_RECORDER_H_
