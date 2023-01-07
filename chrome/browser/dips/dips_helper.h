// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIPS_DIPS_HELPER_H_
#define CHROME_BROWSER_DIPS_DIPS_HELPER_H_

#include "chrome/browser/dips/dips_state.h"
#include "chrome/browser/dips/dips_utils.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace base {
class Clock;
}

class DIPSService;

// A WebContentsObserver subclass that listens for storage and user interaction
// events that DIPSService is interested in.
class DIPSTabHelper : public content::WebContentsObserver,
                      public content::WebContentsUserData<DIPSTabHelper> {
 public:
  DIPSTabHelper(const DIPSTabHelper&) = delete;
  DIPSTabHelper& operator=(const DIPSTabHelper&) = delete;

  // Record that |url| wrote to storage.
  void RecordStorage(const GURL& url);
  // Record that the user interacted on |url| .
  void RecordInteraction(const GURL& url);

  // Posts a blank task to the DIPSStorage SequenceBound, then executes
  // `flushed` after the task finishes.
  void FlushForTesting(base::OnceClosure flushed);

  using StateForURLCallback = base::OnceCallback<void(const DIPSState&)>;
  void StateForURLForTesting(const GURL& url, StateForURLCallback callback);
  static base::Clock* SetClockForTesting(base::Clock* clock);

 private:
  explicit DIPSTabHelper(content::WebContents* web_contents,
                         DIPSService* service);

  DIPSCookieMode GetCookieMode() const;

  // So WebContentsUserData::CreateForWebContents() can call the constructor.
  friend class content::WebContentsUserData<DIPSTabHelper>;

  // WebContentsObserver overrides.
  void FrameReceivedUserActivation(
      content::RenderFrameHost* render_frame_host) override;
  void OnCookiesAccessed(content::RenderFrameHost* render_frame_host,
                         const content::CookieAccessDetails& details) override;
  void OnCookiesAccessed(content::NavigationHandle* navigation_handle,
                         const content::CookieAccessDetails& details) override;

  raw_ptr<DIPSService> service_;
  raw_ptr<base::Clock> clock_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_DIPS_DIPS_HELPER_H_
