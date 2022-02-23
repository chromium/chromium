// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_TAILORED_SECURITY_CHROME_TAILORED_SECURITY_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_TAILORED_SECURITY_CHROME_TAILORED_SECURITY_SERVICE_H_

#include "build/build_config.h"
#include "components/safe_browsing/core/browser/tailored_security_service/tailored_security_service.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/safe_browsing/tailored_security/consented_message_android.h"
#endif

class Profile;

namespace safe_browsing {

class ChromeTailoredSecurityService : public TailoredSecurityService {
 public:
  explicit ChromeTailoredSecurityService(Profile* profile);
  ~ChromeTailoredSecurityService() override;

 protected:
  void MaybeNotifySyncUser(bool is_enabled,
                           base::Time previous_update) override;

  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;

 private:
  void ShowSyncNotification(bool is_enabled);

#if BUILDFLAG(IS_ANDROID)
  void MessageDismissed();

  std::unique_ptr<TailoredSecurityConsentedModalAndroid> message_;
#endif

  Profile* profile_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_TAILORED_SECURITY_CHROME_TAILORED_SECURITY_SERVICE_H_
