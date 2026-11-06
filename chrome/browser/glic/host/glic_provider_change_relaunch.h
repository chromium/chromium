// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GLIC_PROVIDER_CHANGE_RELAUNCH_H_
#define CHROME_BROWSER_GLIC_HOST_GLIC_PROVIDER_CHANGE_RELAUNCH_H_

#include "base/auto_reset.h"
#include "base/functional/callback_forward.h"

class Profile;

namespace glic {

// Relaunches Chrome if `profile` has a pending provider change.
void MaybeRelaunchForProviderChange(Profile* profile);

// Runs `callback` instead of restarting Chrome in
// `MaybeRelaunchForProviderChange()` while the returned object is alive.
[[nodiscard]] base::AutoReset<base::RepeatingClosure>
SetRestartCallbackForTesting(base::RepeatingClosure callback);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GLIC_PROVIDER_CHANGE_RELAUNCH_H_
