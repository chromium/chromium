// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/android_tab_package.h"

namespace tabs {

AndroidTabPackage::AndroidTabPackage(
    int version,
    int id,
    int parent_id,
    long timestamp_millis,
    std::optional<std::vector<uint8_t>> web_contents_state_bytes,
    std::optional<std::string> opener_app_id,
    int theme_color,
    long last_navigation_committed_timestamp_millis,
    bool tab_has_sensitive_content,
    int launch_type_at_creation)
    : version_(version),
      id_(id),
      parent_id_(parent_id),
      timestamp_millis_(timestamp_millis),
      web_contents_state_bytes_(std::move(web_contents_state_bytes)),
      opener_app_id_(std::move(opener_app_id)),
      theme_color_(theme_color),
      last_navigation_committed_timestamp_millis_(
          last_navigation_committed_timestamp_millis),
      tab_has_sensitive_content_(tab_has_sensitive_content),
      launch_type_at_creation_(launch_type_at_creation) {}

AndroidTabPackage::~AndroidTabPackage() = default;

AndroidTabPackage::AndroidTabPackage(AndroidTabPackage&&) noexcept = default;
AndroidTabPackage& AndroidTabPackage::operator=(AndroidTabPackage&&) noexcept =
    default;

}  // namespace tabs
