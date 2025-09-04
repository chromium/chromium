// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CRYPTAUTH_CRYPTAUTH_DEVICE_ID_PROVIDER_H_
#define CHROME_BROWSER_ASH_CRYPTAUTH_CRYPTAUTH_DEVICE_ID_PROVIDER_H_

#include <string>

class PrefRegistrySimple;
class PrefService;

namespace ash::cryptauth_device_id {

// Registers the local state prefs. `registry` must be associated
// with the browser process, not an individual profile.
void RegisterLocalPrefs(PrefRegistrySimple* registry);

// Returns the device ID stored in `local_state`. If the ID is not in the
// `local_state`, this generates a new ID and stores it in `local_state`.
std::string GetDeviceID(PrefService& local_state);

}  // namespace ash::cryptauth_device_id

#endif  // CHROME_BROWSER_ASH_CRYPTAUTH_CRYPTAUTH_DEVICE_ID_PROVIDER_H_
