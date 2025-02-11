// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_PREF_CHANGE_HANDLER_H_
#define CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_PREF_CHANGE_HANDLER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/safe_browsing/tailored_security/chrome_tailored_security_service.h"
#include "chrome/browser/safe_browsing/tailored_security/consented_message_android.h"
#include "chrome/browser/safe_browsing/tailored_security/unconsented_message_android.h"
#endif

class Profile;

namespace safe_browsing {

// Handles showing the appropriate toast or modal when the Safe Browsing
// protection setting changes. This class is not thread-safe.
class SafeBrowsingPrefChangeHandler {
 public:
  SafeBrowsingPrefChangeHandler();
  virtual ~SafeBrowsingPrefChangeHandler();

  // Handles notifying the user when necessary. The type of notification shown
  // depends on the platform and whether the user is currently on the security
  // settings page. Virtual for tests.
  virtual void MaybeShowEnhancedProtectionSettingChangeNotification(
      Profile* profile);

 private:
#if BUILDFLAG(IS_ANDROID)
  // Called when the consented modal is dismissed.
  void ConsentedMessageDismissed();

  // The modal that is shown to the user.
  std::unique_ptr<TailoredSecurityConsentedModalAndroid> message_;
#endif
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_PREF_CHANGE_HANDLER_H_
