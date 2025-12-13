// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/component_updater_prefs.h"

#include "build/build_config.h"
#include "chrome/browser/component_updater/chrome_component_updater_configurator.h"
#include "chrome/browser/component_updater/wasm_tts_engine_component_installer.h"
#include "chrome/common/buildflags.h"
#include "components/component_updater/component_updater_service.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/component_updater/soda_component_installer.h"
#endif

namespace component_updater {

void RegisterPrefs(PrefRegistrySimple* registry) {
  RegisterComponentUpdateServicePrefs(registry);
  WasmTtsEngineComponentInstallerPolicy::RegisterPrefs(registry);
}

}  // namespace component_updater
