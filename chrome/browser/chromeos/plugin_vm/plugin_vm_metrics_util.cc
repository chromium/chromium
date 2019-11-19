// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/plugin_vm/plugin_vm_metrics_util.h"

namespace plugin_vm {

const char kPluginVmImageDownloadedSizeHistogram[] =
    "PluginVm.Image.DownloadedSize";
const char kPluginVmLaunchResultHistogram[] = "PluginVm.LaunchResult";
const char kPluginVmSetupResultHistogram[] = "PluginVm.SetupResult";
const char kPluginVmSetupTimeHistogram[] = "PluginVm.SetupTime";

void RecordPluginVmImageDownloadedSizeHistogram(uint64_t bytes_downloaded) {
  uint64_t megabytes_downloaded = bytes_downloaded / (1024 * 1024);
  base::UmaHistogramMemoryLargeMB(kPluginVmImageDownloadedSizeHistogram,
                                  megabytes_downloaded);
}

void RecordPluginVmLaunchResultHistogram(PluginVmLaunchResult launch_result) {
  base::UmaHistogramEnumeration(kPluginVmLaunchResultHistogram, launch_result);
}

void RecordPluginVmSetupResultHistogram(PluginVmSetupResult setup_result) {
  base::UmaHistogramEnumeration(kPluginVmSetupResultHistogram, setup_result);
}

void RecordPluginVmSetupTimeHistogram(base::TimeDelta setup_time) {
  base::UmaHistogramLongTimes(kPluginVmSetupTimeHistogram, setup_time);
}

}  // namespace plugin_vm
