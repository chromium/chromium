// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_INFO_H_
#define CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_INFO_H_

#include <optional>
#include <string>

#include "base/time/time.h"
#include "services/device/public/mojom/screen_orientation_lock_types.mojom-shared.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
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
             std::string manifest_id,
             blink::mojom::DisplayMode display,
             device::mojom::ScreenOrientationLockType orientation,
             std::optional<SkColor> theme_color,
             std::optional<SkColor> background_color,
             std::optional<SkColor> dark_theme_color,
             std::optional<SkColor> dark_background_color,
             base::Time last_update_check_time,
             base::Time last_update_completion_time,
             bool relax_updates,
             std::string backing_browser_package_name,
             bool is_backing_browser,
             std::string update_status);

  WebApkInfo(const WebApkInfo&) = delete;
  WebApkInfo& operator=(const WebApkInfo&) = delete;

  WebApkInfo& operator=(WebApkInfo&& other) noexcept;
  WebApkInfo(WebApkInfo&& other) noexcept;

  ~WebApkInfo();

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
  std::string manifest_id;
  blink::mojom::DisplayMode display;
  device::mojom::ScreenOrientationLockType orientation;
  std::optional<SkColor> theme_color;
  std::optional<SkColor> background_color;
  std::optional<SkColor> dark_theme_color;
  std::optional<SkColor> dark_background_color;
  base::Time last_update_check_time;
  base::Time last_update_completion_time;
  bool relax_updates;
  std::string backing_browser_package_name;
  bool is_backing_browser;

  // Update Status of the WebAPK.
  std::string update_status;
};

#endif  // CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_INFO_H_
