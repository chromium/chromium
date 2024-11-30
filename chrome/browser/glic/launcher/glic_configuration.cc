// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/launcher/glic_configuration.h"

#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

GlicConfiguration::GlicConfiguration(Observer* manager) : manager_(manager) {
  if (PrefService* local_state = g_browser_process->local_state()) {
    pref_registrar_.Init(local_state);
    pref_registrar_.Add(
        prefs::kGlicLauncherEnabled,
        base::BindRepeating(&GlicConfiguration::OnEnabledPrefChanged,
                            base::Unretained(this)));
  }
}

GlicConfiguration::~GlicConfiguration() = default;

// static
void GlicConfiguration::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kGlicLauncherEnabled, false);
}

bool GlicConfiguration::IsEnabled() {
  return g_browser_process->local_state()->GetBoolean(
      prefs::kGlicLauncherEnabled);
}

void GlicConfiguration::OnEnabledPrefChanged() {
  manager_->OnEnabledChanged(IsEnabled());
}
