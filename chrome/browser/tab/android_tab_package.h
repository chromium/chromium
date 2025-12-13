// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_ANDROID_TAB_PACKAGE_H_
#define CHROME_BROWSER_TAB_ANDROID_TAB_PACKAGE_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace tabs {

// C++ representation of platform-specific tab data for Android.
struct AndroidTabPackage {
 public:
  AndroidTabPackage(
      int version,
      int id,
      int parent_id,
      long timestamp_millis,
      std::optional<std::vector<uint8_t>> web_contents_state_bytes,
      std::optional<std::string> opener_app_id,
      int theme_color,
      long last_navigation_committed_timestamp_millis,
      bool tab_has_sensitive_content,
      int launch_type_at_creation);
  ~AndroidTabPackage();

  AndroidTabPackage(const AndroidTabPackage&) = delete;
  AndroidTabPackage& operator=(const AndroidTabPackage&) = delete;

  AndroidTabPackage(AndroidTabPackage&&) noexcept;
  AndroidTabPackage& operator=(AndroidTabPackage&&) noexcept;

  int version_;
  int id_;
  int parent_id_;
  long timestamp_millis_;
  std::optional<std::vector<uint8_t>> web_contents_state_bytes_;
  std::optional<std::string> opener_app_id_;
  int theme_color_;
  long last_navigation_committed_timestamp_millis_;
  bool tab_has_sensitive_content_;
  int launch_type_at_creation_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_TAB_ANDROID_TAB_PACKAGE_H_
