// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FINDS_CORE_FINDS_TAB_HELPER_H_
#define CHROME_BROWSER_FINDS_CORE_FINDS_TAB_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}

namespace finds {

class FindsService;

// Tab helper to track user opt-in eligibility.
class FindsTabHelper : public content::WebContentsObserver,
                       public content::WebContentsUserData<FindsTabHelper> {
 public:
  FindsTabHelper(const FindsTabHelper&) = delete;
  FindsTabHelper& operator=(const FindsTabHelper&) = delete;
  ~FindsTabHelper() override;

 private:
  explicit FindsTabHelper(content::WebContents* web_contents,
                          FindsService* finds_service);
  friend class content::WebContentsUserData<FindsTabHelper>;

  raw_ptr<FindsService> finds_service_ = nullptr;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace finds

#endif  // CHROME_BROWSER_FINDS_CORE_FINDS_TAB_HELPER_H_
