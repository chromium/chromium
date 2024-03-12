// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_SAFE_BROWSING_AW_SAFE_BROWSING_LOCAL_STATE_DELEGATE_IMPL_H_
#define ANDROID_WEBVIEW_BROWSER_SAFE_BROWSING_AW_SAFE_BROWSING_LOCAL_STATE_DELEGATE_IMPL_H_

#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui.h"

namespace safe_browsing {

class AwSafeBrowsingLocalStateDelegateImpl
    : public SafeBrowsingLocalStateDelegate {
 public:
  AwSafeBrowsingLocalStateDelegateImpl() = default;
  explicit AwSafeBrowsingLocalStateDelegateImpl(content::WebUI* web_ui);
  PrefService* GetLocalState() override;
};

}  // namespace safe_browsing
#endif  // ANDROID_WEBVIEW_BROWSER_SAFE_BROWSING_AW_SAFE_BROWSING_LOCAL_STATE_DELEGATE_IMPL_H_
