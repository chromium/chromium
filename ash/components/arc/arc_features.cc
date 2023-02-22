// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/arc_features.h"
#include "base/feature_list.h"

namespace arc {

// Controls whether to always start ARC automatically, or wait for the user's
// action to start it later in an on-demand manner.
BASE_FEATURE(kArcOnDemandFeature,
             "ArcOnDemand",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls ACTION_BOOT_COMPLETED broadcast for third party applications on ARC.
// When disabled, third party apps will not receive this broadcast.
BASE_FEATURE(kBootCompletedBroadcastFeature,
             "ArcBootCompletedBroadcast",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls experimental Custom Tabs feature for ARC.
BASE_FEATURE(kCustomTabsExperimentFeature,
             "ArcCustomTabsExperiment",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to handle files with unknown size.
BASE_FEATURE(kDocumentsProviderUnknownSizeFeature,
             "ArcDocumentsProviderUnknownSize",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether an Android VPN (ArcHostVpn) should be started when a host
// VPN is started.
BASE_FEATURE(kEnableArcHostVpn,
             "ArcHostVpn",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether we automatically send ARCVM into Doze mode
// when it is mostly idle - even if Chrome is still active.
BASE_FEATURE(kEnableArcIdleManager,
             "ArcIdleManager",
             base::FEATURE_DISABLED_BY_DEFAULT);


// For test purposes, ignore battery status changes, allowing Doze mode to
// kick in even if we do not receive powerd changes related to battery.
const base::FeatureParam<bool> kEnableArcIdleManagerIgnoreBatteryForPLT{
    &kEnableArcIdleManager, "ignore_battery_for_test", false};


// Controls whether files shared to ARC Nearby Share are shared through the
// FuseBox filesystem, instead of the default method (through a temporary path
// managed by file manager).
BASE_FEATURE(kEnableArcNearbyShareFuseBox,
             "ArcNearbyShareFuseBox",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to enable ARCVM /data migration. It does not take effect
// when kEnableVirtioBlkForData is set, in which case virtio-blk is used for
// /data without going through the migration.
BASE_FEATURE(kEnableArcVmDataMigration,
             "ArcVmDataMigration",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether WebView Zygote is lazily initialized in ARC.
BASE_FEATURE(kEnableLazyWebViewInit,
             "LazyWebViewInit",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether crosvm for ARCVM does per-VM core scheduling on devices with
// MDS/L1TF vulnerabilities. When this feature is disabled, crosvm does per-vCPU
// core scheduling which is more secure.
//
// How to safely disable this feature for security (or other) reasons:
//
// 1) Visit go/stainless and verify arc.Boot.vm_with_per_vcpu_core_scheduling is
//    green (it should always be because arc.Boot is a critical test.)
// 2) Change the default value of this feature to FEATURE_DISABLED_BY_DEFAULT.
// 3) Monitor arc.Boot.vm at go/stainless after Chrome is rolled.
// 4) Ask ARC team (//ash/components/arc/OWNERS) to update arc.CPUSet.vm test
//    so the Tast test uses the updated ArcEnablePerVmCoreScheduling setting.
BASE_FEATURE(kEnablePerVmCoreScheduling,
             "ArcEnablePerVmCoreScheduling",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables use of new endpoint for fetching ARC sign-in token.
BASE_FEATURE(kEnableTokenBootstrapEndpoint,
             "ArcEnableTokenBootstrapEndpoint",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to use ARC TTS caching to optimize ARC boot.
BASE_FEATURE(kEnableTTSCaching,
             "ArcEnableTTSCaching",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to use pregenerated ARC TTS cache to optimize ARC boot and
// also whether or not TTS cache is used.
BASE_FEATURE(kEnableTTSCacheSetup,
             "ArcEnableTTSCacheSetup",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether we should delegate audio focus requests from ARC to Chrome.
BASE_FEATURE(kEnableUnifiedAudioFocusFeature,
             "ArcEnableUnifiedAudioFocus",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether ARC handles unmanaged->managed account transition.
BASE_FEATURE(kEnableUnmanagedToManagedTransitionFeature,
             "ArcEnableUnmanagedToManagedTransitionFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls ARC Unspecialized Application Processes.
// When enabled, Android creates a pool of processes
// that will start applications so that zygote doesn't have to wake.
BASE_FEATURE(kEnableUsap, "ArcEnableUsap", base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to use virtio-blk for Android /data instead of using
// virtio-fs.
BASE_FEATURE(kEnableVirtioBlkForData,
             "ArcEnableVirtioBlkForData",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to pop up ghost window for ARC app before fixup finishes.
BASE_FEATURE(kFixupWindowFeature,
             "ArcFixupWindowFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether new UI style for ARC ghost window.
BASE_FEATURE(kGhostWindowNewStyle,
             "ArcGhostWindowNewStyle",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Used for overriding config params for the virtio-blk feature above.
BASE_FEATURE(kVirtioBlkDataConfigOverride,
             "ArcVirtioBlkDataConfigOverride",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to use the LVM-provided disk as the backend device for
// Android /data instead of using the concierge-provided disk.
const base::FeatureParam<bool> kVirtioBlkDataConfigUseLvm{
    &kVirtioBlkDataConfigOverride, "use_lvm", false};

// Indicates whether LVM application containers feature is supported.
BASE_FEATURE(kLvmApplicationContainers,
             "ArcLvmApplicationContainers",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls experimental file picker feature for ARC.
BASE_FEATURE(kFilePickerExperimentFeature,
             "ArcFilePickerExperiment",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether ARCVM can request resourced make more resources available
// for a currently-active ARCVM game.
BASE_FEATURE(kGameModeFeature,
             "ArcGameModeFeature",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether the guest zram is enabled. This is only for ARCVM.
BASE_FEATURE(kGuestZram, "ArcGuestZram", base::FEATURE_DISABLED_BY_DEFAULT);

// Controls the size of the guest zram.
const base::FeatureParam<int> kGuestZramSize{&kGuestZram, "size", 0};

// Controls swappiness for the ARCVM guest.
const base::FeatureParam<int> kGuestZramSwappiness{&kGuestZram, "swappiness",
                                                   0};

// Enables/disables ghost when user launch ARC app from shelf/launcher when
// App already ready for launch.
BASE_FEATURE(kInstantResponseWindowOpen,
             "ArcInstantResponseWindowOpen",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables/disables mlock() of guest memory for ARCVM.
// Often used in combination with kGuestZram.
BASE_FEATURE(kLockGuestMemory,
             "ArcLockGuestMemory",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls keyboard shortcut helper integration feature in ARC.
BASE_FEATURE(kKeyboardShortcutHelperIntegrationFeature,
             "ArcKeyboardShortcutHelperIntegration",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls ARCVM MGLRU reclaim feature.
BASE_FEATURE(kMglruReclaim,
             "ArcMglruReclaim",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls the interval between MGLRU reclaims in milliseconds
// A value of 0 will disable the MGLRU reclaim feature
const base::FeatureParam<int> kMglruReclaimInterval{&kMglruReclaim, "interval",
                                                    0};

// Controls the swappiness of MGLRU reclaims, in the range of 0 to 200
// 0 means only filecache will be used while 200 means only swap will be used
// any value in between will be a mix of both
// The lower the value, the higher the ratio of freeing filecache pages
// Implementation and a more detailed description can be found in ChromeOS
// src/third_party/kernel/v5.10-arcvm/mm/vmscan.c
const base::FeatureParam<int> kMglruReclaimSwappiness{&kMglruReclaim,
                                                      "swappiness", 0};

// Toggles between native bridge implementations for ARC.
// Note, that we keep the original feature name to preserve
// corresponding metrics.
BASE_FEATURE(kNativeBridgeToggleFeature,
             "ArcNativeBridgeExperiment",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, utility processes are spawned to perform hardware decode
// acceleration on behalf of ARC++/ARCVM instead of using the GPU process.
BASE_FEATURE(kOutOfProcessVideoDecoding,
             "OutOfProcessVideoDecoding",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls ARC picture-in-picture feature. If this is enabled, then Android
// will control which apps can enter PIP. If this is disabled, then ARC PIP
// will be disabled.
BASE_FEATURE(kPictureInPictureFeature,
             "ArcPictureInPicture",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls ARCVM real time vcpu feature on a device with 2 logical cores
// online.
// When you change the default, you also need to change the chromeExtraAgas
// in tast-tests/src/chromiumos/tast/local/bundles/cros/arc/cpu_set.go to
// match it to the new default.
BASE_FEATURE(kRtVcpuDualCore,
             "ArcRtVcpuDualCore",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls ARCVM real time vcpu feature on a device with 3+ logical cores
// online.
// When you change the default, you also need to modify the chromeExtraAgas
// in tast-tests/src/chromiumos/tast/local/bundles/cros/arc/cpu_set.go to
// add ArcRtVcpuQuadCore there. Otherwise, the test will start failing.
BASE_FEATURE(kRtVcpuQuadCore,
             "ArcRtVcpuQuadCore",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, tracing raw files are saved in order to help debug failures.
BASE_FEATURE(kSaveRawFilesOnTracing,
             "ArcSaveRawFilesOnTracing",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, CertStoreService will talk to KeyMint instead of Keymaster on
// ARC-T.
// When you change the default, you also need to change whether Keymaster
// or KeyMint is started in ARC. Otherwise, it will not work properly.
BASE_FEATURE(kSwitchToKeyMintOnT,
             "ArcSwitchToKeyMintOnT",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, ARC will pass install priority to Play in sync install
// requests.
BASE_FEATURE(kSyncInstallPriority,
             "ArcSyncInstallPriority",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to update the O4C list via A2C2.
BASE_FEATURE(kArcUpdateO4CListViaA2C2,
             "ArcUpdateO4CListViaA2C2",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls ARC USB Storage UI feature.
// When enabled, chrome://settings and Files.app will ask if the user wants
// to expose USB storage devices to ARC.
BASE_FEATURE(kUsbStorageUIFeature,
             "ArcUsbStorageUI",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls ARC dalvik memory profile in ARCVM.
// When enabled, Android tries to use dalvik memory profile tuned based on the
// device memory size.
BASE_FEATURE(kUseDalvikMemoryProfile,
             "ArcUseDalvikMemoryProfile",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether the system/vendor images are mounted without specifying a
// block size.
BASE_FEATURE(kUseDefaultBlockSize,
             "ArcVmUseDefaultBlockSize",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether ARC uses VideoDecoder-backed video decoding.
// When enabled, GpuArcVideoDecodeAccelerator will use VdVideoDecodeAccelerator
// to delegate decoding tasks to VideoDecoder implementations, instead of using
// VDA implementations created by GpuVideoDecodeAcceleratorFactory.
BASE_FEATURE(kVideoDecoder,
             "ArcVideoDecoder",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Feature to continuously log PSI memory pressure data to Chrome.
BASE_FEATURE(kVmMemoryPSIReports,
             "ArcVmMemoryPSIReports",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls how frequently memory pressure data is logged
const base::FeatureParam<int> kVmMemoryPSIReportsPeriod{&kVmMemoryPSIReports,
                                                        "period", 10};

// Controls whether a custom memory size is used when creating ARCVM. When
// enabled, ARCVM is sized with the following formula:
//  min(max_mib, RAM + shift_mib)
// If disabled, memory is sized by concierge which, at the time of writing, uses
// RAM - 1024 MiB.
BASE_FEATURE(kVmMemorySize,
             "ArcVmMemorySize",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls the amount to "shift" system RAM when sizing ARCVM. The default
// value of 0 means that ARCVM's memory will be thr same as the system.
const base::FeatureParam<int> kVmMemorySizeShiftMiB{&kVmMemorySize, "shift_mib",
                                                    0};

// Controls the maximum amount of memory to give ARCVM. The default value of
// INT32_MAX means that ARCVM's memory is not capped.
const base::FeatureParam<int> kVmMemorySizeMaxMiB{&kVmMemorySize, "max_mib",
                                                  INT32_MAX};

// Controls experimental key GMS Core and related services protection against to
// be killed by low memory killer in ARCVM.
BASE_FEATURE(kVmGmsCoreLowMemoryKillerProtection,
             "ArcVmGmsCoreLowMemoryKillerProtection",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls experimental key to enable pre-ANR handling for BroadcastQueue in
// ARCVM.
BASE_FEATURE(kVmBroadcastPreNotifyANR,
             "ArcVmBroadcastPreAnrHandling",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls experimental key to enable Vmm swap for ARCVM by keyboard shortcut.
BASE_FEATURE(kVmmSwapKeyboardShortcut,
             "ArcvmSwapoutKeyboardShortcut",
             base::FEATURE_DISABLED_BY_DEFAULT);
}  // namespace arc
