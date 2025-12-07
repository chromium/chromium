// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_SAFE_BROWSING_LOCAL_STATE_DELEGATE_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_SAFE_BROWSING_LOCAL_STATE_DELEGATE_H_

#include "components/safe_browsing/core/browser/web_ui/safe_browsing_local_state_delegate.h"

namespace content {
class WebUI;
}

class ChromeSafeBrowsingLocalStateDelegate
    : public safe_browsing::SafeBrowsingLocalStateDelegate {
 public:
  ChromeSafeBrowsingLocalStateDelegate() = default;
  explicit ChromeSafeBrowsingLocalStateDelegate(content::WebUI* web_ui);
  ~ChromeSafeBrowsingLocalStateDelegate() override = default;
  PrefService* GetLocalState() override;
};

#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_SAFE_BROWSING_LOCAL_STATE_DELEGATE_H_
