// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines the public base::FeatureList features for ARC.

#ifndef ASH_COMPONENTS_ARC_ARC_FEATURES_H_
#define ASH_COMPONENTS_ARC_ARC_FEATURES_H_

#include <base/time/time.h>
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace arc {

// Please keep alphabetized.
BASE_DECLARE_FEATURE(kArcExchangeVersionOnMojoHandshake);
BASE_DECLARE_FEATURE(kArcOnDemandV2);
extern const base::FeatureParam<bool> kArcOnDemandActivateOnAppLaunch;
extern const base::FeatureParam<base::TimeDelta> kArcOnDemandInactiveInterval;
BASE_DECLARE_FEATURE(kArcVmGki);
BASE_DECLARE_FEATURE(kBlockIoScheduler);
extern const base::FeatureParam<bool> kEnableDataBlockIoScheduler;
BASE_DECLARE_FEATURE(kBootCompletedBroadcastFeature);
BASE_DECLARE_FEATURE(kContainerAppKiller);
BASE_DECLARE_FEATURE(kCustomTabsExperimentFeature);
BASE_DECLARE_FEATURE(kDeferArcActivationUntilUserSessionStartUpTaskCompletion);
extern const base::FeatureParam<int> kDeferArcActivationHistoryWindow;
extern const base::FeatureParam<int> kDeferArcActivationHistoryThreshold;
BASE_DECLARE_FEATURE(kDocumentsProviderUnknownSizeFeature);
BASE_DECLARE_FEATURE(kEnableArcAttestation);
BASE_DECLARE_FEATURE(kEnableArcIdleManager);
extern const base::FeatureParam<bool> kEnableArcIdleManagerIgnoreBatteryForPLT;
extern const base::FeatureParam<int> kEnableArcIdleManagerDelayMs;
extern const base::FeatureParam<bool>
    kEnableArcIdleManagerPendingIdleReactivate;
BASE_DECLARE_FEATURE(kEnableArcS2Idle);
BASE_DECLARE_FEATURE(kEnableArcVmDataMigration);
BASE_DECLARE_FEATURE(kEnableFriendlierErrorDialog);
BASE_DECLARE_FEATURE(kEnableLazyWebViewInit);
BASE_DECLARE_FEATURE(kEnablePerVmCoreScheduling);
BASE_DECLARE_FEATURE(kEnableReadOnlyPermissions);
BASE_DECLARE_FEATURE(kEnableUnifiedAudioFocusFeature);
BASE_DECLARE_FEATURE(kEnableUnmanagedToManagedTransitionFeature);
BASE_DECLARE_FEATURE(kEnableVirtioBlkForData);
BASE_DECLARE_FEATURE(kEnableVirtioBlkMultipleWorkers);
BASE_DECLARE_FEATURE(kExtendInputAnrTimeout);
BASE_DECLARE_FEATURE(kExtendIntentAnrTimeout);
BASE_DECLARE_FEATURE(kExtendServiceAnrTimeout);
BASE_DECLARE_FEATURE(kExternalStorageAccess);
BASE_DECLARE_FEATURE(kGhostWindowNewStyle);
BASE_DECLARE_FEATURE(kVirtioBlkDataConfigOverride);
extern const base::FeatureParam<bool> kVirtioBlkDataConfigUseLvm;
BASE_DECLARE_FEATURE(kFilePickerExperimentFeature);
BASE_DECLARE_FEATURE(kGmsCoreLowMemoryKillerProtection);
BASE_DECLARE_FEATURE(kGuestSwap);
extern const base::FeatureParam<int> kGuestSwapSize;
extern const base::FeatureParam<int> kGuestZramSizePercentage;
extern const base::FeatureParam<int> kGuestZramSwappiness;
extern const base::FeatureParam<bool> kGuestReclaimEnabled;
extern const base::FeatureParam<bool> kGuestReclaimOnlyAnonymous;
extern const base::FeatureParam<bool> kVirtualSwapEnabled;
extern const base::FeatureParam<int> kVirtualSwapIntervalMs;
BASE_DECLARE_FEATURE(kArcVmPvclock);
BASE_DECLARE_FEATURE(kIgnoreHoverEventAnr);
BASE_DECLARE_FEATURE(kInstantResponseWindowOpen);
BASE_DECLARE_FEATURE(kLockGuestMemory);
BASE_DECLARE_FEATURE(kLvmApplicationContainers);
BASE_DECLARE_FEATURE(kMglruReclaim);
extern const base::FeatureParam<int> kMglruReclaimInterval;
extern const base::FeatureParam<int> kMglruReclaimSwappiness;
BASE_DECLARE_FEATURE(kNativeBridgeToggleFeature);
BASE_DECLARE_FEATURE(kOutOfProcessVideoDecoding);
BASE_DECLARE_FEATURE(kPerAppLanguage);
BASE_DECLARE_FEATURE(kPictureInPictureFeature);
BASE_DECLARE_FEATURE(kResizeCompat);
BASE_DECLARE_FEATURE(kRoundedWindowCompat);
extern const char kRoundedWindowCompatStrategy[];
extern const char kRoundedWindowCompatStrategy_BottomOnlyGesture[];
extern const char kRoundedWindowCompatStrategy_LeftRightBottomGesture[];
BASE_DECLARE_FEATURE(kRtVcpuDualCore);
BASE_DECLARE_FEATURE(kRtVcpuQuadCore);
BASE_DECLARE_FEATURE(kSaveRawFilesOnTracing);
BASE_DECLARE_FEATURE(kSkipDropCaches);
BASE_DECLARE_FEATURE(kSwitchToKeyMintOnT);
BASE_DECLARE_FEATURE(kSwitchToKeyMintOnTOverride);
BASE_DECLARE_FEATURE(kSyncInstallPriority);
BASE_DECLARE_FEATURE(kTouchscreenEmulation);
BASE_DECLARE_FEATURE(kUnthrottleOnActiveAudioV2);
BASE_DECLARE_FEATURE(kUsbStorageUIFeature);
BASE_DECLARE_FEATURE(kUseDalvikMemoryProfile);
BASE_DECLARE_FEATURE(kUseDefaultBlockSize);
BASE_DECLARE_FEATURE(kVideoDecoder);
BASE_DECLARE_FEATURE(kVmMemoryPSIReports);
extern const base::FeatureParam<int> kVmMemoryPSIReportsPeriod;
BASE_DECLARE_FEATURE(kVmMemorySize);
extern const base::FeatureParam<int> kVmMemorySizeShiftMiB;
extern const base::FeatureParam<int> kVmMemorySizeMaxMiB;
extern const base::FeatureParam<int> kVmMemorySizePercentage;
BASE_DECLARE_FEATURE(kVmBroadcastPreNotifyANR);
BASE_DECLARE_FEATURE(kVmmSwapoutGhostWindow);
BASE_DECLARE_FEATURE(kVmmSwapKeyboardShortcut);
BASE_DECLARE_FEATURE(kVmmSwapPolicy);
extern const base::FeatureParam<int> kVmmSwapOutDelaySecond;
extern const base::FeatureParam<int> kVmmSwapOutTimeIntervalSecond;
extern const base::FeatureParam<int> kVmmSwapArcSilenceIntervalSecond;
extern const base::FeatureParam<base::TimeDelta> kVmmSwapTrimInterval;
extern const base::FeatureParam<base::TimeDelta> kVmmSwapMinShrinkInterval;
BASE_DECLARE_FEATURE(kPriorityAppLmkDelay);
extern const base::FeatureParam<int> kPriorityAppLmkDelaySecond;
extern const base::FeatureParam<std::string> kPriorityAppLmkDelayList;
BASE_DECLARE_FEATURE(kLmkPerceptibleMinStateUpdate);
}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_ARC_FEATURES_H_
