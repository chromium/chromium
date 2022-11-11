// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ANDROID_SMS_ANDROID_SMS_URLS_H_
#define CHROME_BROWSER_ASH_ANDROID_SMS_ANDROID_SMS_URLS_H_

#include <ostream>

#include "url/gurl.h"

namespace ash {
namespace android_sms {

enum class PwaDomain {
  kProdAndroid,  // Production, android.com domain.
  kProdGoogle,   // Production, google.com domain.
  kStaging,      // Staging server.
};
std::ostream& operator<<(std::ostream& stream, const PwaDomain& pwa_domain);

// Returns the preferred domain to be used for the Android Messages PWA given
// the currently-enabled flags. In this context, "preferred" refers to the
// domain which is used assuming that a PWA can be successfully installed at
// the associated URL (note that installation fails while offline).
PwaDomain GetPreferredPwaDomain();

// Returns the URL to be used for the Android Messages PWA at a given domain. If
// |use_install_url| is true, the returned URL is used for installation/
// uninstallation of the PWA; otherwise, the returned URL is used for
// ServiceWorker-related tasks (e.g., installing cookies).
GURL GetAndroidMessagesURL(bool use_install_url = false,
                           PwaDomain pwa_domain = GetPreferredPwaDomain());

}  // namespace android_sms
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ANDROID_SMS_ANDROID_SMS_URLS_H_
