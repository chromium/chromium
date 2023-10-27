// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TPCD_HTTP_ERROR_OBSERVER_HTTP_ERROR_TAB_HELPER_H_
#define CHROME_BROWSER_TPCD_HTTP_ERROR_OBSERVER_HTTP_ERROR_TAB_HELPER_H_

#include "base/sequence_checker.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class HttpErrorTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<HttpErrorTabHelper> {
 public:
  HttpErrorTabHelper(const HttpErrorTabHelper&) = delete;
  HttpErrorTabHelper& operator=(const HttpErrorTabHelper&) = delete;
  ~HttpErrorTabHelper() override;

  // WebContentsObserver:
  void ResourceLoadComplete(
      content::RenderFrameHost* render_frame_host,
      const content::GlobalRequestID& request_id,
      const blink::mojom::ResourceLoadInfo& resource_load_info) override;

 protected:
  explicit HttpErrorTabHelper(content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<HttpErrorTabHelper>;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // CHROME_BROWSER_TPCD_HTTP_ERROR_OBSERVER_HTTP_ERROR_TAB_HELPER_H_
