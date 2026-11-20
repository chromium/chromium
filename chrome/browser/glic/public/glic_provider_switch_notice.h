// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_PUBLIC_GLIC_PROVIDER_SWITCH_NOTICE_H_
#define CHROME_BROWSER_GLIC_PUBLIC_GLIC_PROVIDER_SWITCH_NOTICE_H_

#include "base/types/pass_key.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/glic_provider_change.h"

class Profile;

namespace glic {

// This file tracks whether to tell the user, once, that their Glic provider was
// switched for them, e.g. because the admin added, removed or changed the
// `GeminiEnterpriseUrl` policy. The provider is latched per profile at startup
// (`GlicProviderLatchFactory`), so a switch takes effect at a relaunch.
//
// `prefs::kGlicProviderSwitchNoticeKnownProviderKey` holds the provider the
// user last onboarded to. The notice is due when the latched provider differs
// from it.
//
// A switch the user makes themselves from the Glic relaunch prompt, Settings,
// or the in-panel switch API isn't reported: those call
// `RecordUserInitiatedProviderSwitch()` first, directly or through
// `SwitchAssistantSelection()`.

class GlicProviderLatchFactory;

// Updates the known provider for `profile` right after it's latched. While
// `features::kGeicProviderSwitchNotice` or `features::kGeic` is disabled no
// notice is shown, and the known provider follows the latched one, so enabling
// them later doesn't report a long-past switch.
void UpdateKnownProviderAtStartup(base::PassKey<GlicProviderLatchFactory>,
                                  Profile& profile);

// Records the latched provider as known if no provider is known yet and the
// user has onboarded to it. Call when the latched provider's FRE status
// changes.
void OnLatchedProviderFreStatusChanged(Profile& profile);

// Forgets the known provider, so that the switch the user is making themselves
// isn't reported: the next startup records the newly latched provider instead.
// Call right before restarting Chrome to apply the switch. If the restart is
// aborted (e.g. by a `beforeunload` dialog), a switch made for the user before
// the eventual restart isn't reported either.
void RecordUserInitiatedProviderSwitch(Profile& profile);

// Sets the assistant selection for `profile`, which applies at the next
// restart, and records the switch as user-initiated. The caller then restarts
// Chrome.
void SwitchAssistantSelection(Profile& profile,
                              prefs::AssistantSelection selection);

// Returns the switch to tell the user about for `profile`, or `kNone`. Always
// `kNone` while `features::kGeicProviderSwitchNotice` or `features::kGeic` is
// disabled.
GlicProviderChange GetProviderSwitchNotice(Profile& profile);

// Records the latched provider as known, so that the notice isn't shown again.
// Call when the user acknowledges the notice.
void AcknowledgeProviderSwitchNotice(Profile& profile);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_PUBLIC_GLIC_PROVIDER_SWITCH_NOTICE_H_
