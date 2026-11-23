// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GLIC_ASSISTANT_SWITCHER_H_
#define CHROME_BROWSER_GLIC_HOST_GLIC_ASSISTANT_SWITCHER_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/glic/host/glic.mojom-forward.h"

class Profile;

namespace glic {

// Returns whether `profile` is currently eligible to switch to another
// assistant provider via the in-panel switch API.
bool CanSwitchToAnyAssistant(Profile& profile);

// Returns whether `profile` is eligible to switch its active assistant
// provider to `target` via the in-panel switch API.
bool CanSwitchAssistant(Profile& profile, mojom::AssistantTarget target);

// Attempts to switch `profile`'s active assistant selection preference to
// `target` and restart Chrome. Invokes `callback` with the result before
// triggering the browser restart.
void SwitchAssistant(
    Profile& profile,
    mojom::AssistantTarget target,
    base::OnceCallback<void(mojom::SwitchAssistantResponsePtr)> callback);

// Relaunches Chrome if `profile` has a pending provider change.
void MaybeRelaunchForProviderChange(Profile* profile);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GLIC_ASSISTANT_SWITCHER_H_
