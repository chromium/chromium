// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TPCD_METADATA_DEVTOOLS_OBSERVER_H_
#define CHROME_BROWSER_TPCD_METADATA_DEVTOOLS_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content_settings {
class CookieSettings;
}

namespace content_settings {
class CookieSettings;
}

namespace tpcd::metadata {

class Manager;

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

  void OnCookiesAccessedImpl(const content::CookieAccessDetails& details);

  // Emit a devtools issue when `third_party_url` is allowed cookie access as a
  // third-party site on `first_party_url`.
  void EmitMetadataGrantDevtoolsIssue(
      const GURL& third_party_url,
      const GURL& first_party_url,
      const content::CookieAccessDetails::Type cookie_access_type);

  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
  raw_ptr<Manager> tpcd_metadata_manager_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace tpcd::metadata

#endif  // CHROME_BROWSER_TPCD_METADATA_DEVTOOLS_OBSERVER_H_
