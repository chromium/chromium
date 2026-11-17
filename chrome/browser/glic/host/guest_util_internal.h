// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GUEST_UTIL_INTERNAL_H_
#define CHROME_BROWSER_GLIC_HOST_GUEST_UTIL_INTERNAL_H_

// Utilities for the glic guest that should not be used external to
// chrome/browser/glic.

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/glic/host/glic_webui.mojom.h"
#include "url/gurl.h"

class Profile;

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace glic {

class GlicUI;
class Host;

struct BlockedGuestNavigation {
  mojom::GuestPageType page_type;
  GURL blocked_url;
};

// Returns the last paired Workforce Auth Portal URL in `navigation_handle`'s
// redirect chain, or an empty GURL if the navigation has not visited the paired
// Workforce Auth Portal.
GURL GetLastWorkforceAuthPortalUrl(
    content::NavigationHandle& navigation_handle);

// Classifies a guest navigation that is being blocked before commit into
// `kDisabledByAdmin`, `kSignInRequired` (with the last paired Workforce Auth
// Portal URL from the redirect chain), or `kOriginBlocked`.
BlockedGuestNavigation ClassifyBlockedGuestNavigation(
    content::NavigationHandle& navigation_handle);

// Registers `on_geic_sign_in_completed` to run whenever profile-wide GEiC
// sign-in completes with a resolved `csesidx`.
base::CallbackListSubscription RegisterGlicSignInCompletedCallback(
    Profile* profile,
    base::RepeatingClosure on_geic_sign_in_completed);

// Starts the interactive sign-in flow when the user clicks "Sign in" on the
// Glic overlay, dispatching to either the Gemini Enterprise sign-in tab
// (`GeicSignInController`, invoking `on_geic_sign_in_completed` once sign-in
// completes) or Chrome profile primary-account reauth.
void StartGlicSignIn(Profile* profile,
                     content::WebContents* contents,
                     base::OnceClosure on_geic_sign_in_completed);

// Returns the GlicUI for the given guest WebContents, or nullptr if not a Glic
// guest.
GlicUI* GetGlicUiForGuest(content::WebContents* guest_contents);

// Returns the Glic Host for the given guest WebContents, or nullptr if not a
// Glic guest.
Host* GetGlicHostForGuest(content::WebContents* guest_contents);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GUEST_UTIL_INTERNAL_H_
