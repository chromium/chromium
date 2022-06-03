// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUBRESOURCE_REDIRECT_HTTPS_IMAGE_COMPRESSION_INFOBAR_DECIDER_H_
#define CHROME_BROWSER_SUBRESOURCE_REDIRECT_HTTPS_IMAGE_COMPRESSION_INFOBAR_DECIDER_H_

#include "base/sequence_checker.h"

class PrefService;

namespace content {
class NavigationHandle;
}

namespace data_reduction_proxy {
class DataReductionProxySettings;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

// This specifies an interface for deciding to show the InfoBar that notifies
// the user that LiteMode now also optimizes images in HTTPS pages.
class HttpsImageCompressionInfoBarDecider {
 public:
  HttpsImageCompressionInfoBarDecider(
      PrefService* pref_service,
      data_reduction_proxy::DataReductionProxySettings* drp_settings);

  HttpsImageCompressionInfoBarDecider(
      const HttpsImageCompressionInfoBarDecider&) = delete;
  HttpsImageCompressionInfoBarDecider& operator=(
      const HttpsImageCompressionInfoBarDecider&) = delete;

  // Registers the prefs used in this class.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Returns true if the infobar needs to be shown to the user before this https
  // image compression can be applied.
  bool NeedToShowInfoBar();

  // Returns whether the infobar can be shown for the |navigation_handle|.
  // Infobar should not be shown for non-https, CCT pages, etc.
  bool CanShowInfoBar(content::NavigationHandle* navigation_handle);

  // Sets that the user has seen the infobar.
  void SetUserHasSeenInfoBar();

 private:
  // A reference to the profile's |PrefService|.
  PrefService* pref_service_ = nullptr;

  // Whether the infobar infobar needs to be shown to the user.
  bool need_to_show_infobar_ = false;

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // CHROME_BROWSER_SUBRESOURCE_REDIRECT_HTTPS_IMAGE_COMPRESSION_INFOBAR_DECIDER_H_
