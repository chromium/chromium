// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/android_sms/android_sms_urls.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "base/command_line.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ash/android_sms/android_sms_switches.h"
#include "url/gurl.h"

namespace ash {
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

GURL GetCustomDomain(bool use_install_url) {
  std::string custom_domain =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kCustomAndroidMessagesDomain);
  if (custom_domain.empty()) {
    return GURL();
  }

  GURL url(custom_domain);
  GURL::Replacements path;
  if (use_install_url) {
    path.SetPathStr("/web/authentication");
  } else {  // App url.
    path.SetPathStr("/web/");
  }

  return url.ReplaceComponents(path);
}

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

  return PwaDomain::kProdGoogle;
}

GURL GetAndroidMessagesURL(bool use_install_url, PwaDomain pwa_domain) {
  // If present, use custom override for the preferred domain.
  if (pwa_domain == GetPreferredPwaDomain()) {
    GURL custom_url = GetCustomDomain(use_install_url);
    if (!custom_url.is_empty())
      return custom_url;
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
}  // namespace ash
