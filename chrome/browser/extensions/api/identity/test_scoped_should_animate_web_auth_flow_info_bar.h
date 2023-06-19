// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IDENTITY_TEST_SCOPED_SHOULD_ANIMATE_WEB_AUTH_FLOW_INFO_BAR_H_
#define CHROME_BROWSER_EXTENSIONS_API_IDENTITY_TEST_SCOPED_SHOULD_ANIMATE_WEB_AUTH_FLOW_INFO_BAR_H_

#include "chrome/browser/extensions/api/identity/web_auth_flow_info_bar_delegate.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace extensions {

class TestScopedShouldAnimateWebAuthFlowInfoBar {
 public:
  explicit TestScopedShouldAnimateWebAuthFlowInfoBar(bool should_animate) {
    previous_state_ = WebAuthFlowInfoBarDelegate::should_animate_for_testing_;
    WebAuthFlowInfoBarDelegate::should_animate_for_testing_ = should_animate;
  }

  ~TestScopedShouldAnimateWebAuthFlowInfoBar() {
    WebAuthFlowInfoBarDelegate::should_animate_for_testing_ = previous_state_;
  }

 private:
  absl::optional<bool> previous_state_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IDENTITY_TEST_SCOPED_SHOULD_ANIMATE_WEB_AUTH_FLOW_INFO_BAR_H_
