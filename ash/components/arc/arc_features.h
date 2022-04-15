// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines the public base::FeatureList features for ARC.

#ifndef ASH_COMPONENTS_ARC_ARC_FEATURES_H_
#define ASH_COMPONENTS_ARC_ARC_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace arc {

// Please keep alphabetized.
extern const base::Feature kBootCompletedBroadcastFeature;
extern const base::Feature kCompatSnapFeature;
extern const base::Feature kCustomTabsExperimentFeature;
extern const base::Feature kDocumentsProviderUnknownSizeFeature;
extern const base::Feature kEnableArcNearbyShare;
extern const base::Feature kEnablePerVmCoreScheduling;
extern const base::Feature kEnableThrottlingNotification;
extern const base::Feature kEnableTTSCaching;
extern const base::Feature kEnableUnifiedAudioFocusFeature;
extern const base::Feature kEnableUnmanagedToManagedTransitionFeature;
extern const base::Feature kEnableUsap;
extern const base::Feature kEnableVirtioBlkForData;
extern const base::Feature kFilePickerExperimentFeature;
extern const base::Feature kGmsCoreLowMemoryKillerProtection;
extern const base::Feature kGuestZram;
extern const base::FeatureParam<int> kGuestZramSize;
extern const base::Feature kLogdConfig;
extern const base::FeatureParam<int> kLogdConfigSize;
extern const base::Feature kKeyboardShortcutHelperIntegrationFeature;
extern const base::Feature kNativeBridge64BitSupportExperimentFeature;
extern const base::Feature kNativeBridgeToggleFeature;
extern const base::Feature kOutOfProcessVideoDecoding;
extern const base::Feature kPictureInPictureFeature;
extern const base::Feature kRightClickLongPress;
extern const base::Feature kRtVcpuDualCore;
extern const base::Feature kRtVcpuQuadCore;
extern const base::Feature kSaveRawFilesOnTracing;
extern const base::Feature kUsbDeviceDefaultAttachToArcVm;
extern const base::Feature kUsbStorageUIFeature;
extern const base::Feature kUseDalvikMemoryProfile;
extern const base::Feature kUseDefaultBlockSize;
extern const base::Feature kVideoDecoder;
extern const base::Feature kVmMemoryPSIReports;
extern const base::FeatureParam<int> kVmMemoryPSIReportsPeriod;
extern const base::Feature kVmMemorySize;
extern const base::FeatureParam<int> kVmMemorySizeShiftMiB;
extern const base::FeatureParam<int> kVmMemorySizeMaxMiB;
extern const base::Feature kVmBalloonPolicy;
extern const base::FeatureParam<int> kVmBalloonPolicyModerateKiB;
extern const base::FeatureParam<int> kVmBalloonPolicyCriticalKiB;
extern const base::FeatureParam<int> kVmBalloonPolicyReclaimKiB;
extern const base::Feature kVmGmsCoreLowMemoryKillerProtection;

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_ARC_FEATURES_H_
