// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PREFETCH_CHROME_PREFETCH_MANAGER_H_
#define CHROME_BROWSER_PRELOADING_PREFETCH_CHROME_PREFETCH_MANAGER_H_

#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

// Manages all prefetch triggers from the //chrome.
class ChromePrefetchManager
    : public content::WebContentsUserData<ChromePrefetchManager> {
 public:
  ChromePrefetchManager(const ChromePrefetchManager&) = delete;
  ChromePrefetchManager& operator=(const ChromePrefetchManager&) = delete;

  ~ChromePrefetchManager() override;

  static ChromePrefetchManager* GetOrCreateForWebContents(
      content::WebContents* web_contents);

#if BUILDFLAG(IS_ANDROID)
  void StartPrefetchFromCCT(const GURL& prefetch_url,
                            bool use_prefetch_proxy,
                            const std::optional<url::Origin>& referring_origin);
#endif  // BUILDFLAG(IS_ANDROID)

 private:
  explicit ChromePrefetchManager(content::WebContents* web_contents);
  friend class content::WebContentsUserData<ChromePrefetchManager>;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_PRELOADING_PREFETCH_CHROME_PREFETCH_MANAGER_H_
