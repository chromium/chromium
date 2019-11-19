// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_INFO_H_
#define CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_INFO_H_

#include <string>

#include "base/macros.h"
#include "base/time/time.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/common/screen_orientation/web_screen_orientation_lock_type.h"
#include "third_party/skia/include/core/SkColor.h"

// Structure with information about a WebAPK.
//
// This class is passed around in a std::vector to generate the chrome://webapks
// page. To reduce copying overhead, this class is move-only, and
// move-constructs its string arguments (which are copied from Java to C++ into
// a temporary prior to construction).
struct WebApkInfo {
  WebApkInfo(std::string name,
             std::string short_name,
             std::string package_name,
             std::string id,
             int shell_apk_version,
             int version_code,
             std::string uri,
             std::string scope,
             std::string manifest_url,
             std::string manifest_start_url,
             blink::mojom::DisplayMode display,
             blink::WebScreenOrientationLockType orientation,
             base::Optional<SkColor> theme_color,
             base::Optional<SkColor> background_color,
             base::Time last_update_check_time,
             base::Time last_update_completion_time,
             bool relax_updates,
             std::string backing_browser_package_name,
             bool is_backing_browser,
             std::string update_status);
  ~WebApkInfo();

  WebApkInfo& operator=(WebApkInfo&& other) noexcept;
  WebApkInfo(WebApkInfo&& other) noexcept;

  // Short name of the WebAPK.
  std::string name;

  // Short name of the WebAPK.
  std::string short_name;

  // Package name of the WebAPK.
  std::string package_name;

  // Internal ID of the WebAPK.
  std::string id;

  // Shell APK version of the WebAPK.
  int shell_apk_version;

  // Version code of the WebAPK.
  int version_code;

  std::string uri;
  std::string scope;
  std::string manifest_url;
  std::string manifest_start_url;
  blink::mojom::DisplayMode display;
  blink::WebScreenOrientationLockType orientation;
  base::Optional<SkColor> theme_color;
  base::Optional<SkColor> background_color;
  base::Time last_update_check_time;
  base::Time last_update_completion_time;
  bool relax_updates;
  std::string backing_browser_package_name;
  bool is_backing_browser;

  // Update Status of the WebAPK.
  std::string update_status;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebApkInfo);
};

#endif  // CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_INFO_H_
