// Copyright 2018 The Chromium Authors
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

BASE_DECLARE_FEATURE(kClosedTabCache);

BASE_DECLARE_FEATURE(kDestroyProfileOnBrowserClose);
BASE_DECLARE_FEATURE(kDestroySystemProfiles);

BASE_DECLARE_FEATURE(kDevToolsTabTarget);

BASE_DECLARE_FEATURE(kKeepToolbarTexture);

BASE_DECLARE_FEATURE(kNukeProfileBeforeCreateMultiAsync);

BASE_DECLARE_FEATURE(kPromoBrowserCommands);
extern const char kBrowserCommandIdParam[];

#if BUILDFLAG(IS_CHROMEOS_ASH)
BASE_DECLARE_FEATURE(kQuickSettingsPWANotifications);
#endif

#if BUILDFLAG(IS_CHROMEOS)
BASE_DECLARE_FEATURE(kDoubleTapToZoomInTabletMode);
#endif

#if BUILDFLAG(IS_MAC)
BASE_DECLARE_FEATURE(kEnableUniveralLinks);
#endif

#if !BUILDFLAG(IS_ANDROID)
BASE_DECLARE_FEATURE(kCopyLinkToText);
BASE_DECLARE_FEATURE(kMuteNotificationSnoozeAction);
#endif

BASE_DECLARE_FEATURE(kSandboxExternalProtocolBlocked);
BASE_DECLARE_FEATURE(kSandboxExternalProtocolBlockedWarning);
BASE_DECLARE_FEATURE(kTriggerNetworkDataMigration);

#if BUILDFLAG(IS_CHROMEOS)
BASE_DECLARE_FEATURE(kTabCaptureBlueBorderCrOS);
#endif

BASE_DECLARE_FEATURE(kWebUsbDeviceDetection);

#if BUILDFLAG(IS_ANDROID)
BASE_DECLARE_FEATURE(kCertificateTransparencyAndroid);
#endif

BASE_DECLARE_FEATURE(kLargeFaviconFromGoogle);
extern const base::FeatureParam<int> kLargeFaviconFromGoogleSizeInDip;

BASE_DECLARE_FEATURE(kObserverBasedPostProfileInit);

BASE_DECLARE_FEATURE(kRestartNetworkServiceUnsandboxedForFailedLaunch);

BASE_DECLARE_FEATURE(kKeyPinningComponentUpdater);

#if BUILDFLAG(IS_WIN)
BASE_DECLARE_FEATURE(kAppBoundEncryptionMetrics);
#endif

BASE_DECLARE_FEATURE(kFlexOrgManagementDisclosure);

BASE_DECLARE_FEATURE(kFedCmWithoutThirdPartyCookies);

BASE_DECLARE_FEATURE(kIncomingCallNotifications);

}  // namespace features

#endif  // CHROME_BROWSER_BROWSER_FEATURES_H_
