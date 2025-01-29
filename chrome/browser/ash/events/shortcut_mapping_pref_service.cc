// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/events/shortcut_mapping_pref_service.h"

#include "ash/constants/ash_pref_names.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "components/prefs/pref_service.h"

namespace ash {

ShortcutMappingPrefService::ShortcutMappingPrefService(PrefService& local_state)
    : local_state_(local_state) {}

ShortcutMappingPrefService::~ShortcutMappingPrefService() = default;

bool ShortcutMappingPrefService::IsDeviceEnterpriseManaged() const {
  return InstallAttributes::Get()->IsEnterpriseManaged();
}

bool ShortcutMappingPrefService::IsI18nShortcutPrefEnabled() const {
  const auto* pref =
      local_state_->FindPreference(prefs::kDeviceI18nShortcutsEnabled);
  CHECK(pref);

  return pref->GetValue()->GetBool();
}

}  // namespace ash
