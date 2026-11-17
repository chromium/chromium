// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GEMINI_ENTERPRISE_GEIC_SIGN_IN_CONTROLLER_H_
#define CHROME_BROWSER_GLIC_GEMINI_ENTERPRISE_GEIC_SIGN_IN_CONTROLLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "url/gurl.h"
#include "url/origin.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace glic {

class GeicManagedTab;

// Profile-scoped controller for Workforce Identity (BYOID / Gemini Business)
// browser-tab sign-in initiated from the Glic overlay UI when a guest
// navigation is intercepted at the paired Workforce Auth Portal before commit.
//
// Storing `GeicSignInController` on `Profile` (via `SupportsUserData`) shares
// the resolved `csesidx` session index across Glic instances in the same
// profile and allows an in-progress sign-in tab (and its `SignInObserver`) to
// survive `Host::Hibernate()`.
class GeicSignInController : public base::SupportsUserData::Data {
 public:
  explicit GeicSignInController(Profile* profile);
  GeicSignInController(const GeicSignInController&) = delete;
  GeicSignInController& operator=(const GeicSignInController&) = delete;
  ~GeicSignInController() override;

  // Returns the `GeicSignInController` for `profile`, creating it if needed.
  static GeicSignInController* GetForProfile(Profile* profile);

  // Returns the `GeicSignInController` for `profile` if one has already been
  // created, or nullptr otherwise.
  static GeicSignInController* GetIfExists(Profile* profile);

  // Records `sign_in_url` when a guest navigation is cancelled because it
  // requires interactive sign-in, and clears any previously resolved `csesidx`.
  void OnSignInRequired(const GURL& sign_in_url);

  // Registers `callback` to be notified whenever profile-wide sign-in
  // completes with a resolved `csesidx`.
  base::CallbackListSubscription RegisterSignInCompletedCallback(
      base::RepeatingClosure callback);

  // Appends the resolved `csesidx` query parameter (if any) to `guest_url`.
  GURL ResolveGuestUrl(GURL guest_url) const;

  // Opens (or activates) the managed sign-in tab for `guest_url` and observes
  // it for sign-in completion, invoking `on_completed` (along with any other
  // registered callbacks) once the auth redirect chain commits back on the
  // guest origin with a non-empty `csesidx`.
  void StartSignIn(const GURL& guest_url, base::OnceClosure on_completed);

  const GURL& last_intercepted_signin_url_for_testing() const {
    return last_intercepted_signin_url_;
  }

  void ObserveSignInContentsForTesting(content::WebContents* web_contents,
                                       const url::Origin& guest_origin,
                                       base::OnceClosure on_completed);

 private:
  class SignInObserver;

  void StartObserving(content::WebContents* web_contents,
                      const url::Origin& guest_origin,
                      base::OnceClosure on_completed);
  void OnSignInTabCompleted(std::string csesidx);
  void OnSignInTabDestroyed();

  raw_ptr<Profile> profile_ = nullptr;
  GURL last_intercepted_signin_url_;
  std::string csesidx_;
  base::RepeatingClosureList sign_in_completed_callbacks_;
  std::vector<base::OnceClosure> on_signin_callbacks_;
  std::unique_ptr<GeicManagedTab> signin_tab_;
  std::unique_ptr<SignInObserver> signin_observer_;

  base::WeakPtrFactory<GeicSignInController> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GEMINI_ENTERPRISE_GEIC_SIGN_IN_CONTROLLER_H_
