// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_WEB_CONTENTS_DATA_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_WEB_CONTENTS_DATA_H_

#include "base/memory/raw_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace apps {

// AppWebContentsData is attached to the lifetime of a WebContents, and notifies
// Observer when the WebContents is destroyed.
class AppWebContentsData
    : public content::WebContentsUserData<AppWebContentsData>,
      public content::WebContentsObserver {
 public:
  class Client {
   public:
    // Invoked when the WebContents is being destroyed.
    virtual void OnWebContentsDestroyed(content::WebContents* contents) = 0;
  };

  explicit AppWebContentsData(content::WebContents* contents, Client* client);
  AppWebContentsData(const AppWebContentsData&) = delete;
  AppWebContentsData& operator=(const AppWebContentsData&) = delete;
  ~AppWebContentsData() override = default;

 private:
  friend class content::WebContentsUserData<AppWebContentsData>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  // content::WebContentsObserver:
  void WebContentsDestroyed() override;

  raw_ptr<Client> client_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_WEB_CONTENTS_DATA_H_
