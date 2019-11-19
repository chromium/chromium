// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/android_sms/android_sms_urls.h"

#include <string>

#include "base/command_line.h"
#include "chrome/browser/chromeos/android_sms/android_sms_switches.h"
#include "chromeos/constants/chromeos_features.h"
#include "url/gurl.h"

namespace chromeos {

namespace android_sms {

namespace {

// Note: Install and app URLs are the same for the android.com domain.
const char kProdAndroidUrl[] = "https://messages.android.com/";

const char kProdGoogleAppUrl[] = "https://messages.google.com/web/";
const char kProdGoogleInstallUrl[] =
    "https://messages.google.com/web/authentication";

const char kStagingAppUrl[] = "https://messages-web.sandbox.google.com/web/";
const char kStagingInstallUrl[] =
    "https://messages-web.sandbox.google.com/web/authentication";

}  // namespace

std::ostream& operator<<(std::ostream& stream, const PwaDomain& pwa_domain) {
  switch (pwa_domain) {
    case PwaDomain::kProdAndroid:
      stream << "[Production: messages.android.com]";
      break;
    case PwaDomain::kProdGoogle:
      stream << "[Production: messages.google.com]";
      break;
    case PwaDomain::kStaging:
      stream << "[Staging: messages-web.sandbox.google.com]";
      break;
  }
  return stream;
}

PwaDomain GetPreferredPwaDomain() {
  if (base::FeatureList::IsEnabled(features::kUseMessagesStagingUrl))
    return PwaDomain::kStaging;

  if (base::FeatureList::IsEnabled(features::kUseMessagesGoogleComDomain))
    return PwaDomain::kProdGoogle;

  return PwaDomain::kProdAndroid;
}

GURL GetAndroidMessagesURL(bool use_install_url, PwaDomain pwa_domain) {
  // If present, use commandline override for the preferred domain.
  if (pwa_domain == GetPreferredPwaDomain()) {
    std::string url_from_command_line_arg;
    if (use_install_url) {
      url_from_command_line_arg =
          base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
              switches::kAlternateAndroidMessagesInstallUrl);
    } else {
      url_from_command_line_arg =
          base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
              switches::kAlternateAndroidMessagesUrl);
    }

    if (!url_from_command_line_arg.empty())
      return GURL(url_from_command_line_arg);
  }

  switch (pwa_domain) {
    case PwaDomain::kProdAndroid:
      return GURL(kProdAndroidUrl);
    case PwaDomain::kProdGoogle:
      return use_install_url ? GURL(kProdGoogleInstallUrl)
                             : GURL(kProdGoogleAppUrl);
    case PwaDomain::kStaging:
      return use_install_url ? GURL(kStagingInstallUrl) : GURL(kStagingAppUrl);
  }
}

}  // namespace android_sms

}  // namespace chromeos
