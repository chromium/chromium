// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GPU_GPU_MODE_MANAGER_H_
#define CHROME_BROWSER_GPU_GPU_MODE_MANAGER_H_

#include "components/prefs/pref_change_registrar.h"

class PrefRegistrySimple;

class GpuModeManager {
 public:
  static void RegisterPrefs(PrefRegistrySimple* registry);

  GpuModeManager();

  GpuModeManager(const GpuModeManager&) = delete;
  GpuModeManager& operator=(const GpuModeManager&) = delete;

  ~GpuModeManager();

  bool initial_gpu_mode_pref() const;

 private:
  static bool IsGpuModePrefEnabled();

  PrefChangeRegistrar pref_registrar_;

  bool initial_gpu_mode_pref_;
};

#endif  // CHROME_BROWSER_GPU_GPU_MODE_MANAGER_H_
