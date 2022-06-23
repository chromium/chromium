// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines the browser-specific base::FeatureList features that are
// not shared with other process types.

#ifndef CHROME_BROWSER_BROWSER_FEATURES_H_
#define CHROME_BROWSER_BROWSER_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.

extern const base::Feature kClosedTabCache;

extern const base::Feature kColorProviderRedirectionForThemeProvider;

extern const base::Feature kDestroyProfileOnBrowserClose;
extern const base::Feature kDestroySystemProfiles;

extern const base::Feature kNukeProfileBeforeCreateMultiAsync;

extern const base::Feature kPromoBrowserCommands;
extern const char kBrowserCommandIdParam[];

extern const base::Feature kUseManagementService;

#if BUILDFLAG(IS_CHROMEOS_ASH)
extern const base::Feature kQuickSettingsPWANotifications;
#endif

#if BUILDFLAG(IS_CHROMEOS)
extern const base::Feature kDoubleTapToZoomInTabletMode;
#endif

#if BUILDFLAG(IS_MAC)
extern const base::Feature kEnableUniveralLinks;
#endif

#if !BUILDFLAG(IS_ANDROID)
extern const base::Feature kCopyLinkToText;
extern const base::Feature kMuteNotificationSnoozeAction;
#endif

extern const base::Feature kSandboxExternalProtocolBlocked;
extern const base::Feature kSandboxExternalProtocolBlockedWarning;
extern const base::Feature kTriggerNetworkDataMigration;

extern const base::Feature kWebUsbDeviceDetection;

#if BUILDFLAG(IS_ANDROID)
extern const base::Feature kCertificateTransparencyAndroid;
#endif

extern const base::Feature kLargeFaviconFromGoogle;
extern const base::FeatureParam<int> kLargeFaviconFromGoogleSizeInDip;

extern const base::Feature kObserverBasedPostProfileInit;

extern const base::Feature kRestartNetworkServiceUnsandboxedForFailedLaunch;

extern const base::Feature kKeyPinningComponentUpdater;

#if BUILDFLAG(IS_WIN)
extern const base::Feature kAppBoundEncryptionMetrics;
#endif

}  // namespace features

#endif  // CHROME_BROWSER_BROWSER_FEATURES_H_
