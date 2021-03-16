// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/component_updater/loader_policies/origin_trials_component_loader_policy.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "android_webview/browser/aw_browser_process.h"
#include "base/containers/flat_map.h"
#include "base/values.h"
#include "base/version.h"
#include "components/component_updater/installer_policies/origin_trials_component_installer.h"
#include "components/embedder_support/origin_trials/component_updater_utils.h"

namespace android_webview {

OriginTrialsComponentLoaderPolicy::OriginTrialsComponentLoaderPolicy() =
    default;

OriginTrialsComponentLoaderPolicy::~OriginTrialsComponentLoaderPolicy() =
    default;

void OriginTrialsComponentLoaderPolicy::ComponentLoaded(
    const base::Version& version,
    const base::flat_map<std::string, int>& fd_map,
    std::unique_ptr<base::DictionaryValue> manifest) {
  // Read the configuration from the manifest and set values in browser
  // local_state. These will be used on the next browser restart.
  // If an individual configuration value is missing, treat as a reset to the
  // browser defaults.
  embedder_support::ReadOriginTrialsConfigAndPopulateLocalState(
      android_webview::AwBrowserProcess::GetInstance()->local_state(),
      std::move(manifest));
}

void OriginTrialsComponentLoaderPolicy::ComponentLoadFailed() {}

void OriginTrialsComponentLoaderPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  component_updater::OriginTrialsComponentInstallerPolicy::GetComponentHash(
      hash);
}

}  // namespace android_webview
