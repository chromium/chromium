// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_TAILORED_SECURITY_CHROME_TAILORED_SECURITY_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_TAILORED_SECURITY_CHROME_TAILORED_SECURITY_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/safe_browsing/core/browser/tailored_security_service/tailored_security_service.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/safe_browsing/tailored_security/consented_message_android.h"
#endif

class Browser;
class Profile;
namespace content {
class WebContents;
}

namespace safe_browsing {

class ChromeTailoredSecurityService : public TailoredSecurityService {
 public:
  explicit ChromeTailoredSecurityService(Profile* profile);
  ~ChromeTailoredSecurityService() override;

 protected:
  void MaybeNotifySyncUser(bool is_enabled,
                           base::Time previous_update) override;

#if !BUILDFLAG(IS_ANDROID)
  // Shows a dialog on the provided `web_contents`. If `show_enable_dialog` is
  // true, display the enabled dialog; otherwise show the disabled dialog. This
  // method is virtual to support testing.
  virtual void DisplayDesktopDialog(Browser* browser,
                                    content::WebContents* web_contents,
                                    bool show_enable_dialog);
#endif

  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;

 private:
  void ShowSyncNotification(bool is_enabled);

#if BUILDFLAG(IS_ANDROID)
  void MessageDismissed();

  std::unique_ptr<TailoredSecurityConsentedModalAndroid> message_;
#endif

  raw_ptr<Profile> profile_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_TAILORED_SECURITY_CHROME_TAILORED_SECURITY_SERVICE_H_
