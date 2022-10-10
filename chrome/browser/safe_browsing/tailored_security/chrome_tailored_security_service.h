// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_TAILORED_SECURITY_CHROME_TAILORED_SECURITY_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_TAILORED_SECURITY_CHROME_TAILORED_SECURITY_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/safe_browsing/core/browser/tailored_security_service/tailored_security_service.h"
#include "components/safe_browsing/core/browser/tailored_security_service/tailored_security_service_observer.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/safe_browsing/tailored_security/consented_message_android.h"
#else
#include "chrome/browser/ui/views/safe_browsing/tailored_security_desktop_dialog_manager.h"
#endif

class Browser;
class Profile;

namespace safe_browsing {

class ChromeTailoredSecurityService : public TailoredSecurityService,
                                      public TailoredSecurityServiceObserver {
 public:
  explicit ChromeTailoredSecurityService(Profile* profile);
  ~ChromeTailoredSecurityService() override;

  // TailoredSecurityServiceObserver.
  void OnSyncNotificationMessageRequest(bool is_enabled) override;

 protected:
#if !BUILDFLAG(IS_ANDROID)
  // Shows a dialog on the provided `browser`. If `show_enable_dialog` is
  // true, display the enabled dialog; otherwise show the disabled dialog. This
  // method is virtual to support testing.
  virtual void DisplayDesktopDialog(Browser* browser, bool show_enable_dialog);
#endif

  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;

 private:
#if BUILDFLAG(IS_ANDROID)
  void MessageDismissed();

  std::unique_ptr<TailoredSecurityConsentedModalAndroid> message_;
#else
  TailoredSecurityDesktopDialogManager dialog_manager_;
#endif

  raw_ptr<Profile> profile_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_TAILORED_SECURITY_CHROME_TAILORED_SECURITY_SERVICE_H_
