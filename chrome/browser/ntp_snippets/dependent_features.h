// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NTP_SNIPPETS_DEPENDENT_FEATURES_H_
#define CHROME_BROWSER_NTP_SNIPPETS_DEPENDENT_FEATURES_H_

#include "components/ntp_snippets/features.h"

namespace ntp_snippets {

bool IsSimplifiedNtpEnabled();

bool AreAssetDownloadsEnabled();
bool AreOfflinePageDownloadsEnabled();
bool IsDownloadsProviderEnabled();

bool IsBookmarkProviderEnabled();

bool IsPhysicalWebPageProviderEnabled();

}  // namespace ntp_snippets

#endif  // CHROME_BROWSER_NTP_SNIPPETS_DEPENDENT_FEATURES_H_
