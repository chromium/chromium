// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_ANDROID_TAB_PACKAGE_H_
#define CHROME_BROWSER_TAB_ANDROID_TAB_PACKAGE_H_

#include <memory>
#include <string>

namespace tabs {

// C++ representation of platform-specific tab data for Android.
struct AndroidTabPackage {
 public:
  AndroidTabPackage(int version,
                    int id,
                    int parent_id,
                    long timestamp_millis,
                    std::unique_ptr<std::string> web_contents_state_bytes,
                    std::unique_ptr<std::string> opener_app_id,
                    int theme_color,
                    long last_navigation_committed_timestamp_millis,
                    bool tab_has_sensitive_content,
                    int launch_type_at_creation);
  ~AndroidTabPackage();

  AndroidTabPackage(const AndroidTabPackage&) = delete;
  AndroidTabPackage& operator=(const AndroidTabPackage&) = delete;

  const int version_;
  const int id_;
  const int parent_id_;
  const long timestamp_millis_;
  const std::unique_ptr<std::string> web_contents_state_bytes_;
  const std::unique_ptr<std::string> opener_app_id_;
  const int theme_color_;
  const long last_navigation_committed_timestamp_millis_;
  const bool tab_has_sensitive_content_;
  const int launch_type_at_creation_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_TAB_ANDROID_TAB_PACKAGE_H_
