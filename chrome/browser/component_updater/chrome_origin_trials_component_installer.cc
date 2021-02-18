// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/chrome_origin_trials_component_installer.h"

#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace component_updater {

namespace {

static const char kManifestPublicKeyPath[] = "origin-trials.public-key";
static const char kManifestDisabledFeaturesPath[] =
    "origin-trials.disabled-features";
static const char kManifestDisabledTokenSignaturesPath[] =
    "origin-trials.disabled-tokens.signatures";

}  // namespace

void ChromeOriginTrialsComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    std::unique_ptr<base::DictionaryValue> manifest) {
  // Read the configuration from the manifest and set values in browser
  // local_state. These will be used on the next browser restart.
  // If an individual configuration value is missing, treat as a reset to the
  // browser defaults.
  PrefService* local_state = g_browser_process->local_state();
  std::string override_public_key;
  if (manifest->GetString(kManifestPublicKeyPath, &override_public_key)) {
    local_state->Set(prefs::kOriginTrialPublicKey,
                     base::Value(override_public_key));
  } else {
    local_state->ClearPref(prefs::kOriginTrialPublicKey);
  }
  base::ListValue* override_disabled_feature_list = nullptr;
  const bool manifest_has_disabled_features = manifest->GetList(
      kManifestDisabledFeaturesPath, &override_disabled_feature_list);
  if (manifest_has_disabled_features &&
      !override_disabled_feature_list->empty()) {
    ListPrefUpdate update(local_state, prefs::kOriginTrialDisabledFeatures);
    update->Swap(override_disabled_feature_list);
  } else {
    local_state->ClearPref(prefs::kOriginTrialDisabledFeatures);
  }
  base::ListValue* disabled_tokens_list = nullptr;
  const bool manifest_has_disabled_tokens = manifest->GetList(
      kManifestDisabledTokenSignaturesPath, &disabled_tokens_list);
  if (manifest_has_disabled_tokens && !disabled_tokens_list->empty()) {
    ListPrefUpdate update(local_state, prefs::kOriginTrialDisabledTokens);
    update->Swap(disabled_tokens_list);
  } else {
    local_state->ClearPref(prefs::kOriginTrialDisabledTokens);
  }
}

void RegisterOriginTrialsComponent(ComponentUpdateService* updater_service) {
  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<ChromeOriginTrialsComponentInstallerPolicy>());
  installer->Register(updater_service, base::OnceClosure());
}

}  // namespace component_updater
