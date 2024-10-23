// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_SAFE_BROWSING_AW_SAFE_BROWSING_UI_H_
#define ANDROID_WEBVIEW_BROWSER_SAFE_BROWSING_AW_SAFE_BROWSING_UI_H_

#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui.h"
#include "content/public/browser/web_ui.h"

namespace safe_browsing {

class AWSafeBrowsingUI : public SafeBrowsingUI {
 public:
  AWSafeBrowsingUI(content::WebUI* web_ui);

  AWSafeBrowsingUI(const AWSafeBrowsingUI&) = delete;
  AWSafeBrowsingUI& operator=(const AWSafeBrowsingUI&) = delete;

  ~AWSafeBrowsingUI() override;
};

}  // namespace safe_browsing

#endif  // ANDROID_WEBVIEW_BROWSER_SAFE_BROWSING_AW_SAFE_BROWSING_UI_H_
