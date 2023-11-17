// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_PROFILE_LOADER_H_
#define CHROME_BROWSER_LACROS_PROFILE_LOADER_H_

#include <cstdint>

#include "base/functional/callback_forward.h"

class Profile;

// Asynchronously loads the primary profile (from disk, if needed), calling
// `callback` with the profile once it is initialized. If loading fails, the
// profile picker UI will be shown.
//
// If `can_trigger_fre` is true, the First Run Experience may be shown
// before `callback` is called. FRE should be triggered for any flow which
// launches a Lacros browser.
//
// `callback` will be called with nullptr if loading fails, or if FRE is
// triggered but exits unsuccessfully.
void LoadMainProfile(base::OnceCallback<void(Profile*)> callback,
                     bool can_trigger_fre);

// Similarly to `LoadMainProfile`, this asynchronously loads the profile that
// matches `profile_id`. When `profile_id` is zero(unset), it loads the primary
// profile. When the matching profile is not found, it shows the profile picker
// UI.
void LoadProfileWithId(base::OnceCallback<void(Profile*)> callback,
                       bool can_trigger_fre,
                       uint64_t profile_id);

#endif  // CHROME_BROWSER_LACROS_PROFILE_LOADER_H_
