// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_TAILORED_SECURITY_TAILORED_SECURITY_URL_OBSERVER_H_
#define CHROME_BROWSER_SAFE_BROWSING_TAILORED_SECURITY_TAILORED_SECURITY_URL_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/safe_browsing/core/browser/tailored_security_service/tailored_security_service_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/safe_browsing/tailored_security/unconsented_message_android.h"
#endif

namespace content {
class RenderWidgetHost;
}  // namespace content

namespace safe_browsing {

class TailoredSecurityService;

// This class handles the observation of whether the user is on a Google
// property or not, so we can query the user's Tailored Security setting when
// they are on a Google property.
class TailoredSecurityUrlObserver
    : public TailoredSecurityServiceObserver,
      public content::WebContentsObserver,
      public content::WebContentsUserData<TailoredSecurityUrlObserver> {
 public:
  TailoredSecurityUrlObserver(const TailoredSecurityUrlObserver&) = delete;
  TailoredSecurityUrlObserver& operator=(const TailoredSecurityUrlObserver&) =
      delete;

  ~TailoredSecurityUrlObserver() override;

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;
  void OnWebContentsFocused(
      content::RenderWidgetHost* render_widget_host) override;
  void OnWebContentsLostFocus(
      content::RenderWidgetHost* render_widget_host) override;

  // TailoredSecurityServiceObserver
  void OnTailoredSecurityBitChanged(bool enabled,
                                    base::Time previous_update) override;
  void OnTailoredSecurityServiceDestroyed() override;

 private:
  TailoredSecurityUrlObserver(content::WebContents* web_contents,
                              TailoredSecurityService* service);

  void UpdateFocusAndURL(bool focused, const GURL& url);

  friend class content::WebContentsUserData<TailoredSecurityUrlObserver>;

#if BUILDFLAG(IS_ANDROID)
  void MessageDismissed();

  std::unique_ptr<TailoredSecurityUnconsentedMessageAndroid> message_;
#endif

  // Reference to the TailoredSecurityService for this profile.
  raw_ptr<TailoredSecurityService> service_;

  // Whether the WebContents is currently in focus.
  bool focused_ = false;

  // The most recent URL the WebContents navigated to.
  GURL last_url_;

  // Whether we currently have a query request.
  bool has_query_request_ = false;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_TAILORED_SECURITY_TAILORED_SECURITY_URL_OBSERVER_H_
