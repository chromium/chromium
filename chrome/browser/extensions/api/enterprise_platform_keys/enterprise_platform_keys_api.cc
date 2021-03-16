// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains code that is shared between Ash and Lacros.
// Lacros/Ash-specific counterparts are implemented in separate files.

#include "chrome/browser/extensions/api/enterprise_platform_keys/enterprise_platform_keys_api.h"

#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"

namespace extensions {
namespace platform_keys {

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kAttestationExtensionAllowlist);
}

bool IsExtensionAllowed(Profile* profile, const Extension* extension) {
  if (Manifest::IsComponentLocation(extension->location())) {
    // Note: For this to even be called, the component extension must also be
    // allowed in chrome/common/extensions/api/_permission_features.json
    return true;
  }
  const base::ListValue* list =
      profile->GetPrefs()->GetList(prefs::kAttestationExtensionAllowlist);
  DCHECK_NE(list, nullptr);
  base::Value value(extension->id());
  return list->Find(value) != list->end();
}

}  // namespace platform_keys
}  // namespace extensions
