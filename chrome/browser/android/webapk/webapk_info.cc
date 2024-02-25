// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chrome/browser/android/webapk/webapk_info.h"

WebApkInfo::WebApkInfo(std::string name,
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
                       std::string update_status)
    : name(std::move(name)),
      short_name(std::move(short_name)),
      package_name(std::move(package_name)),
      id(std::move(id)),
      shell_apk_version(shell_apk_version),
      version_code(version_code),
      uri(std::move(uri)),
      scope(std::move(scope)),
      manifest_url(std::move(manifest_url)),
      manifest_start_url(std::move(manifest_start_url)),
      manifest_id(std::move(manifest_id)),
      display(display),
      orientation(orientation),
      theme_color(theme_color),
      background_color(background_color),
      dark_theme_color(dark_theme_color),
      dark_background_color(dark_background_color),
      last_update_check_time(last_update_check_time),
      last_update_completion_time(last_update_completion_time),
      relax_updates(relax_updates),
      backing_browser_package_name(std::move(backing_browser_package_name)),
      is_backing_browser(is_backing_browser),
      update_status(std::move(update_status)) {}

WebApkInfo::~WebApkInfo() {}

WebApkInfo& WebApkInfo::operator=(WebApkInfo&& rhs) noexcept = default;
WebApkInfo::WebApkInfo(WebApkInfo&& other) noexcept = default;
