// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_KERNEL_FEATURE_MANAGER_H_
#define CHROME_BROWSER_ASH_SYSTEM_KERNEL_FEATURE_MANAGER_H_

#include <string>
#include <utility>
#include <vector>

#include "base/memory/weak_ptr.h"

namespace chromeos {
class DebugDaemonClient;
}

namespace ash {

// It is desirable to test kernel patches on a population to measure benefits
// or problems with application of a kernel patch. For experimentation,
// a patch or tuning can be applied to all kernels, and finch will enable it on
// subset of the user population. An experiment has an ‘enablement method’.
// This method can involve writing to sysfs, procfs entry, or executing
// other commands, via debugd. This class manages the enabling of these
// kernel experiments, when Chrome starts, via ChromeOS's debugd. The behavior
// of the running kernel is changed live without requiring a restart.
class KernelFeatureManager {
 public:
  explicit KernelFeatureManager(
      chromeos::DebugDaemonClient* debug_daemon_client);
  KernelFeatureManager(const KernelFeatureManager&) = delete;
  KernelFeatureManager& operator=(const KernelFeatureManager&) = delete;
  ~KernelFeatureManager();

 private:
  void OnDebugDaemonReady(bool service_is_ready);

  // Get a list of kernel features that debugd can enable. The list of features
  // are passed to the callback in |out| as a comma separate value string, or
  // an error string containing the reason for failure. |result| contains true
  // or false depending on if the request succeeds or fails.
  void GetKernelFeatureList();
  void OnKernelFeatureList(bool result, const std::string& out);

  // Enable all kernel features that were requested to be enabled in field trial
  // experiments. A callback is called for each feature that is enabled, with
  // |result| containing true or false depending on if the request succeeds or
  // fails. |out| contains an error string on failure and name of the feature on
  // success.
  void EnableKernelFeatures();
  void OnKernelFeatureEnable(bool result, const std::string& out);

  chromeos::DebugDaemonClient* const debug_daemon_client_;
  bool debug_daemon_ready_ = false;
  std::vector<std::string> kernel_feature_list_;

  base::WeakPtrFactory<KernelFeatureManager> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_KERNEL_FEATURE_MANAGER_H_
