// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines the browser-specific base::FeatureList features that are
// not shared with other process types.

#ifndef CHROME_BROWSER_BROWSER_FEATURES_H_
#define CHROME_BROWSER_BROWSER_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.

extern const base::Feature kClosedTabCache;

extern const base::Feature kColorProviderRedirectionForThemeProvider;

extern const base::Feature kDestroyProfileOnBrowserClose;

extern const base::Feature kNukeProfileBeforeCreateMultiAsync;

extern const base::Feature kPromoBrowserCommands;
extern const char kBrowserCommandIdParam[];

extern const base::Feature kUseManagementService;

#if BUILDFLAG(IS_CHROMEOS_ASH)
extern const base::Feature kDoubleTapToZoomInTabletMode;
extern const base::Feature kQuickSettingsPWANotifications;
#endif

#if defined(OS_MAC)
extern const base::Feature kEnableUniveralLinks;
#endif

#if !defined(OS_ANDROID)
extern const base::Feature kCopyLinkToText;
extern const base::Feature kMuteNotificationSnoozeAction;
#endif

#if defined(OS_WIN)
extern const base::Feature kPrewarmSearchResultsPageFonts;
#endif

extern const base::Feature kPwaUpdateDialogForNameAndIcon;

extern const base::Feature kSandboxExternalProtocolBlocked;
extern const base::Feature kTabCaptureBlueBorder;
extern const base::Feature kTabCaptureBlueBorderForSelfCaptureRegionCaptureOT;
extern const base::Feature kTriggerNetworkDataMigration;

extern const base::Feature kWebUsbDeviceDetection;

#if defined(OS_ANDROID)
extern const base::Feature kCertificateTransparencyAndroid;
#endif

extern const base::Feature kLargeFaviconFromGoogle;
extern const base::FeatureParam<int> kLargeFaviconFromGoogleSizeInDip;

}  // namespace features

#endif  // CHROME_BROWSER_BROWSER_FEATURES_H_
