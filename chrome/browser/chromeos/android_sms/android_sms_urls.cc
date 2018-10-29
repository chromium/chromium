// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/android_sms/android_sms_urls.h"

#include <string>

#include "base/command_line.h"
#include "base/optional.h"
#include "chrome/browser/chromeos/android_sms/android_sms_switches.h"
#include "chromeos/chromeos_features.h"
#include "url/gurl.h"

namespace chromeos {

namespace android_sms {

namespace {

// NOTE: Using internal staging server until changes roll out to prod.
const char kAndroidMessagesSandboxUrl[] =
    "https://android-messages.sandbox.google.com/";

const char kAndroidMessagesProdUrl[] = "https://messages.android.com/";

const char kUrlParams[] = "?DefaultToPersistent=true";

GURL GetURLInternal(bool with_params) {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  std::string url_string =
      command_line->GetSwitchValueASCII(switches::kAlternateAndroidMessagesUrl);

  bool use_prod_url = base::FeatureList::IsEnabled(
      chromeos::features::kAndroidMessagesProdEndpoint);
  if (url_string.empty())
    url_string = std::string(use_prod_url ? kAndroidMessagesProdUrl
                                          : kAndroidMessagesSandboxUrl);
  if (with_params)
    url_string += kUrlParams;
  return GURL(url_string);
}

}  // namespace

GURL GetAndroidMessagesURL() {
  return GetURLInternal(false /* with_params */);
}

GURL GetAndroidMessagesURLWithParams() {
  return GetURLInternal(true /* with_params */);
}

}  // namespace android_sms

}  // namespace chromeos
