// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_TAB_OBSERVER_H_
#define CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_TAB_OBSERVER_H_

#include <memory>

#include "base/macros.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}

namespace safe_browsing {

class ClientSideDetectionHost;

// Per-tab class to handle safe-browsing functionality.
class SafeBrowsingTabObserver
    : public content::WebContentsUserData<SafeBrowsingTabObserver> {
 public:
  ~SafeBrowsingTabObserver() override;

 private:
  explicit SafeBrowsingTabObserver(content::WebContents* web_contents);
  friend class content::WebContentsUserData<SafeBrowsingTabObserver>;

  // Internal helpers ----------------------------------------------------------

  // Create or destroy SafebrowsingDetectionHost as needed if the user's
  // safe browsing preference has changed.
  void UpdateSafebrowsingDetectionHost();

  // Handles IPCs.
  std::unique_ptr<ClientSideDetectionHost> safebrowsing_detection_host_;

  // Our owning WebContents.
  content::WebContents* web_contents_;

  PrefChangeRegistrar pref_change_registrar_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingTabObserver);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_TAB_OBSERVER_H_
