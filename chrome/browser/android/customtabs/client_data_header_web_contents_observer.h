// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_CUSTOMTABS_CLIENT_DATA_HEADER_WEB_CONTENTS_OBSERVER_H_
#define CHROME_BROWSER_ANDROID_CUSTOMTABS_CLIENT_DATA_HEADER_WEB_CONTENTS_OBSERVER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace customtabs {

class ClientDataHeaderWebContentsObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<ClientDataHeaderWebContentsObserver> {
 public:
  ~ClientDataHeaderWebContentsObserver() override;

  // Called on creation and also when the tab switches out from CCT.
  void SetHeader(const std::string& header);

  std::string header() { return header_; }

 private:
  friend class content::WebContentsUserData<
      ClientDataHeaderWebContentsObserver>;

  explicit ClientDataHeaderWebContentsObserver(
      content::WebContents* web_contents);

  // content::WebContentsObserver implementation:
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;

  void UpdateFrameCCTHeader(content::RenderFrameHost* render_frame_host);

  std::string header_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace customtabs

#endif  // CHROME_BROWSER_ANDROID_CUSTOMTABS_CLIENT_DATA_HEADER_WEB_CONTENTS_OBSERVER_H_
