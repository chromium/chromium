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
BASE_DECLARE_FEATURE(kBootCompletedBroadcastFeature);
BASE_DECLARE_FEATURE(kCustomTabsExperimentFeature);
BASE_DECLARE_FEATURE(kDocumentsProviderUnknownSizeFeature);
BASE_DECLARE_FEATURE(kEnableArcNearbyShareFuseBox);
BASE_DECLARE_FEATURE(kEnablePerVmCoreScheduling);
BASE_DECLARE_FEATURE(kEnableTTSCaching);
BASE_DECLARE_FEATURE(kEnableTTSCacheSetup);
BASE_DECLARE_FEATURE(kEnableUnifiedAudioFocusFeature);
BASE_DECLARE_FEATURE(kEnableUnmanagedToManagedTransitionFeature);
BASE_DECLARE_FEATURE(kEnableUsap);
BASE_DECLARE_FEATURE(kEnableVirtioBlkForData);
BASE_DECLARE_FEATURE(kFixupWindowFeature);
BASE_DECLARE_FEATURE(kVirtioBlkDataConfigOverride);
extern const base::FeatureParam<bool> kVirtioBlkDataConfigUseLvm;
BASE_DECLARE_FEATURE(kFilePickerExperimentFeature);
BASE_DECLARE_FEATURE(kGameModeFeature);
BASE_DECLARE_FEATURE(kGmsCoreLowMemoryKillerProtection);
BASE_DECLARE_FEATURE(kGuestZram);
extern const base::FeatureParam<int> kGuestZramSize;
extern const base::FeatureParam<int> kGuestZramSwappiness;
BASE_DECLARE_FEATURE(kLockGuestMemory);
BASE_DECLARE_FEATURE(kLogdConfig);
extern const base::FeatureParam<int> kLogdConfigSize;
BASE_DECLARE_FEATURE(kLvmApplicationContainers);
BASE_DECLARE_FEATURE(kKeyboardShortcutHelperIntegrationFeature);
BASE_DECLARE_FEATURE(kNativeBridgeToggleFeature);
BASE_DECLARE_FEATURE(kOutOfProcessVideoDecoding);
BASE_DECLARE_FEATURE(kPictureInPictureFeature);
BASE_DECLARE_FEATURE(kRightClickLongPress);
BASE_DECLARE_FEATURE(kRtVcpuDualCore);
BASE_DECLARE_FEATURE(kRtVcpuQuadCore);
BASE_DECLARE_FEATURE(kSaveRawFilesOnTracing);
BASE_DECLARE_FEATURE(kUsbDeviceDefaultAttachToArcVm);
BASE_DECLARE_FEATURE(kUsbStorageUIFeature);
BASE_DECLARE_FEATURE(kUseDalvikMemoryProfile);
BASE_DECLARE_FEATURE(kUseDefaultBlockSize);
BASE_DECLARE_FEATURE(kVideoDecoder);
BASE_DECLARE_FEATURE(kVmMemoryPSIReports);
extern const base::FeatureParam<int> kVmMemoryPSIReportsPeriod;
BASE_DECLARE_FEATURE(kVmMemorySize);
extern const base::FeatureParam<int> kVmMemorySizeShiftMiB;
extern const base::FeatureParam<int> kVmMemorySizeMaxMiB;
BASE_DECLARE_FEATURE(kVmBalloonPolicy);
extern const base::FeatureParam<int> kVmBalloonPolicyModerateKiB;
extern const base::FeatureParam<int> kVmBalloonPolicyCriticalKiB;
extern const base::FeatureParam<int> kVmBalloonPolicyReclaimKiB;
BASE_DECLARE_FEATURE(kVmBroadcastPreNotifyANR);
BASE_DECLARE_FEATURE(kVmGmsCoreLowMemoryKillerProtection);
extern const base::FeatureParam<bool> kVmBalloonPolicyResponsive;
extern const base::FeatureParam<int> kVmBalloonPolicyResponsiveTimeoutMs;
extern const base::FeatureParam<int> kVmBalloonPolicyResponsiveMaxDeflateBytes;

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_ARC_FEATURES_H_
