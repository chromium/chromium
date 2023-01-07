// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PARTNER_BROWSER_CUSTOMIZATIONS_H_
#define CHROME_BROWSER_ANDROID_PARTNER_BROWSER_CUSTOMIZATIONS_H_

namespace chrome {
namespace android {

class PartnerBrowserCustomizations {
 public:
  PartnerBrowserCustomizations() = delete;
  PartnerBrowserCustomizations(const PartnerBrowserCustomizations&) = delete;
  PartnerBrowserCustomizations& operator=(const PartnerBrowserCustomizations&) =
      delete;

  // Whether incognito mode is disabled by the partner.
  static bool IsIncognitoDisabled();
};

}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_PARTNER_BROWSER_CUSTOMIZATIONS_H_
