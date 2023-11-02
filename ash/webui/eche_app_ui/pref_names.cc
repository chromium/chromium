// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/pref_names.h"

namespace ash {
namespace eche_app {
namespace prefs {
// The last provided apps access status provided by the phone. This pref
// stores the numerical value associated with the
// phonehub::MultideviceFeatureAccessManager::AccessStatus enum.
const char kAppsAccessStatus[] = "cros.echeapps.apps_access_status";
}  // namespace prefs
}  // namespace eche_app
}  // namespace ash
