// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_DEPRECATED_NOTICES_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_DEPRECATED_NOTICES_H_

#include <array>
#include <string_view>

#include "chrome/browser/privacy_sandbox/notice/notice_model.h"

namespace privacy_sandbox {

// A registry of deprecated notices.
// These notice IDs and storage names (feature names) must NEVER be reused
// to prevent data corruption and conflicts in local preferences.
struct DeprecatedNotice {
  NoticeId notice_id;
  std::string_view storage_name;
};

// The Deprecated Notices list: Add any fully deprecated notices here.
inline constexpr std::array<DeprecatedNotice, 0> kDeprecatedNotices = {};

}  // namespace privacy_sandbox

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_DEPRECATED_NOTICES_H_