// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_LINK_CAPTURING_LINK_CAPTURING_TAB_HELPER_H_
#define CHROME_BROWSER_APPS_LINK_CAPTURING_LINK_CAPTURING_TAB_HELPER_H_

#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents_user_data.h"

namespace apps {

// TabHelper for storing link capturing data that cannot otherwise be inferred
// during a navigation.
class LinkCapturingTabHelper
    : public content::WebContentsUserData<LinkCapturingTabHelper> {
 public:
  ~LinkCapturingTabHelper() override;

  const webapps::AppId& source_app_id() const { return source_app_id_; }

 private:
  LinkCapturingTabHelper(content::WebContents* contents,
                         webapps::AppId source_app_id);

  friend WebContentsUserData;
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  // The App ID of the web app where the link that caused this tab to open was
  // clicked.
  const webapps::AppId source_app_id_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_LINK_CAPTURING_LINK_CAPTURING_TAB_HELPER_H_
