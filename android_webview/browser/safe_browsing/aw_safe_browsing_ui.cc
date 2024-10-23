// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/safe_browsing/aw_safe_browsing_ui.h"

#include "android_webview/browser/safe_browsing/aw_safe_browsing_local_state_delegate_impl.h"

namespace safe_browsing {

AWSafeBrowsingUI::AWSafeBrowsingUI(content::WebUI* web_ui)
    : SafeBrowsingUI(
          web_ui,
          std::make_unique<safe_browsing::AwSafeBrowsingLocalStateDelegateImpl>(
              web_ui)) {}

AWSafeBrowsingUI::~AWSafeBrowsingUI() = default;

}  // namespace safe_browsing
