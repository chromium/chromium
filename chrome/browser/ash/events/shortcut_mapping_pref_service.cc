// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/events/shortcut_mapping_pref_service.h"

#include "ash/constants/ash_pref_names.h"
#include "chrome/browser/browser_process.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "components/prefs/pref_service.h"

namespace ash {

ShortcutMappingPrefService::ShortcutMappingPrefService() = default;
ShortcutMappingPrefService::~ShortcutMappingPrefService() = default;

bool ShortcutMappingPrefService::IsDeviceEnterpriseManaged() const {
  return InstallAttributes::Get()->IsEnterpriseManaged();
}

bool ShortcutMappingPrefService::IsI18nShortcutPrefEnabled() const {
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);

  const auto* pref =
      local_state->FindPreference(prefs::kDeviceI18nShortcutsEnabled);
  DCHECK(pref);

  return pref->GetValue()->GetBool();
}

}  // namespace ash
