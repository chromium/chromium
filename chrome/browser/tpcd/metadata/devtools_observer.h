// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TPCD_METADATA_DEVTOOLS_OBSERVER_H_
#define CHROME_BROWSER_TPCD_METADATA_DEVTOOLS_OBSERVER_H_

#include "chrome/browser/dips/dips_service.h"
#include "chrome/browser/tpcd/heuristics/opener_heuristic_tab_helper.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

namespace tpcd::metadata {

class TpcdMetadataDevtoolsObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<TpcdMetadataDevtoolsObserver> {
 public:
  explicit TpcdMetadataDevtoolsObserver(content::WebContents* web_contents);
  TpcdMetadataDevtoolsObserver() = delete;
  ~TpcdMetadataDevtoolsObserver() override;

 private:
  // To allow use of WebContentsUserData::CreateForWebContents()
  friend class content::WebContentsUserData<TpcdMetadataDevtoolsObserver>;

  // WebContentsObserver overrides:
  void OnCookiesAccessed(content::RenderFrameHost* render_frame_host,
                         const content::CookieAccessDetails& details) override;
  void OnCookiesAccessed(content::NavigationHandle* navigation_handle,
                         const content::CookieAccessDetails& details) override;

  void OnCookiesAccessedImpl(const GURL& url, const GURL& first_party_url);

  // Emit a devtools issue when `url` is allowed cookie access as a third-party
  // site on the current page.
  void EmitMetadataGrantDevtoolsIssue(const GURL& url);

  scoped_refptr<content_settings::CookieSettings> cookie_settings_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace tpcd::metadata

#endif  // CHROME_BROWSER_TPCD_METADATA_DEVTOOLS_OBSERVER_H_
