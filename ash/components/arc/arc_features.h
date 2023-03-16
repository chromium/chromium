// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines the public base::FeatureList features for ARC.

#ifndef ASH_COMPONENTS_ARC_ARC_FEATURES_H_
#define ASH_COMPONENTS_ARC_ARC_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace arc {

// Please keep alphabetized.
BASE_DECLARE_FEATURE(kArcOnDemandFeature);
BASE_DECLARE_FEATURE(kBootCompletedBroadcastFeature);
BASE_DECLARE_FEATURE(kCustomTabsExperimentFeature);
BASE_DECLARE_FEATURE(kDocumentsProviderUnknownSizeFeature);
BASE_DECLARE_FEATURE(kEnableArcHostVpn);
BASE_DECLARE_FEATURE(kEnableArcIdleManager);
extern const base::FeatureParam<bool> kEnableArcIdleManagerIgnoreBatteryForPLT;
BASE_DECLARE_FEATURE(kEnableArcNearbyShareFuseBox);
BASE_DECLARE_FEATURE(kEnableArcVmDataMigration);
BASE_DECLARE_FEATURE(kEnableLazyWebViewInit);
BASE_DECLARE_FEATURE(kEnablePerVmCoreScheduling);
BASE_DECLARE_FEATURE(kEnableUnifiedAudioFocusFeature);
BASE_DECLARE_FEATURE(kEnableUnmanagedToManagedTransitionFeature);
BASE_DECLARE_FEATURE(kEnableUsap);
BASE_DECLARE_FEATURE(kEnableVirtioBlkForData);
BASE_DECLARE_FEATURE(kFixupWindowFeature);
BASE_DECLARE_FEATURE(kGhostWindowNewStyle);
BASE_DECLARE_FEATURE(kVirtioBlkDataConfigOverride);
extern const base::FeatureParam<bool> kVirtioBlkDataConfigUseLvm;
BASE_DECLARE_FEATURE(kFilePickerExperimentFeature);
BASE_DECLARE_FEATURE(kGameModeFeature);
BASE_DECLARE_FEATURE(kGmsCoreLowMemoryKillerProtection);
BASE_DECLARE_FEATURE(kGuestZram);
extern const base::FeatureParam<int> kGuestZramSize;
extern const base::FeatureParam<int> kGuestZramSwappiness;
extern const base::FeatureParam<bool> kGuestReclaimEnabled;
extern const base::FeatureParam<bool> kGuestReclaimOnlyAnonymous;
BASE_DECLARE_FEATURE(kInstantResponseWindowOpen);
BASE_DECLARE_FEATURE(kLockGuestMemory);
BASE_DECLARE_FEATURE(kLvmApplicationContainers);
BASE_DECLARE_FEATURE(kKeyboardShortcutHelperIntegrationFeature);
BASE_DECLARE_FEATURE(kMglruReclaim);
extern const base::FeatureParam<int> kMglruReclaimInterval;
extern const base::FeatureParam<int> kMglruReclaimSwappiness;
BASE_DECLARE_FEATURE(kNativeBridgeToggleFeature);
BASE_DECLARE_FEATURE(kOutOfProcessVideoDecoding);
BASE_DECLARE_FEATURE(kPictureInPictureFeature);
BASE_DECLARE_FEATURE(kRightClickLongPress);
BASE_DECLARE_FEATURE(kRtVcpuDualCore);
BASE_DECLARE_FEATURE(kRtVcpuQuadCore);
BASE_DECLARE_FEATURE(kSaveRawFilesOnTracing);
BASE_DECLARE_FEATURE(kSwitchToKeyMintOnT);
BASE_DECLARE_FEATURE(kSyncInstallPriority);
BASE_DECLARE_FEATURE(kArcUpdateO4CListViaA2C2);
BASE_DECLARE_FEATURE(kUsbStorageUIFeature);
BASE_DECLARE_FEATURE(kUseDalvikMemoryProfile);
BASE_DECLARE_FEATURE(kUseDefaultBlockSize);
BASE_DECLARE_FEATURE(kVideoDecoder);
BASE_DECLARE_FEATURE(kVmMemoryPSIReports);
extern const base::FeatureParam<int> kVmMemoryPSIReportsPeriod;
BASE_DECLARE_FEATURE(kVmMemorySize);
extern const base::FeatureParam<int> kVmMemorySizeShiftMiB;
extern const base::FeatureParam<int> kVmMemorySizeMaxMiB;
BASE_DECLARE_FEATURE(kVmBroadcastPreNotifyANR);
BASE_DECLARE_FEATURE(kVmmSwapKeyboardShortcut);
BASE_DECLARE_FEATURE(kVmmSwapPolicy);
extern const base::FeatureParam<int> kVmmSwapOutDelaySecond;
extern const base::FeatureParam<int> kVmmSwapOutTimeIntervalSecond;
extern const base::FeatureParam<int> kVmmSwapArcSilenceIntervalSecond;

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_ARC_FEATURES_H_
