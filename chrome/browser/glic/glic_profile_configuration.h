// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_PROFILE_CONFIGURATION_H_
#define CHROME_BROWSER_GLIC_GLIC_PROFILE_CONFIGURATION_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/base/accelerators/accelerator.h"

class PrefRegistrySimple;

namespace glic {

// This class manages interaction with the prefs system for Glic per-profile
// settings. For launcher settings, which are per-installation, see
// GlicLauncherConfiguration.
class GlicProfileConfiguration {
 public:
  GlicProfileConfiguration() = default;
  ~GlicProfileConfiguration();

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);
};
}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_PROFILE_CONFIGURATION_H_
