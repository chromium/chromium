// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_PUBLIC_GLIC_FRE_STORE_H_
#define CHROME_BROWSER_GLIC_PUBLIC_GLIC_FRE_STORE_H_

#include <string_view>

#include "chrome/browser/glic/glic_pref_names.h"

class Profile;

namespace glic {

// Per-provider storage and behavior for the FRE. Get the store for a profile
// from `GlicProviderSpec::GetFreStore()`, and read and write the status through
// `GlicEnabling` (`GetCompletedFre()`, `SetCompletedFre()`).
//
// Implementations hold no state: the status lives in the profile's prefs.
class GlicFreStore {
 public:
  GlicFreStore() = default;
  GlicFreStore(const GlicFreStore&) = delete;
  GlicFreStore& operator=(const GlicFreStore&) = delete;
  virtual ~GlicFreStore() = default;

  // Returns the FRE status for `profile`.
  virtual prefs::FreStatus GetStatus(Profile* profile) const = 0;

  // Sets the FRE status for `profile`. Statuses the FRE doesn't support are
  // ignored.
  virtual void SetStatus(Profile* profile, prefs::FreStatus status) const = 0;

  // Completes the FRE for `profile` when the web client reports onboarding
  // completed, including this FRE's side effects (e.g. metrics, OS launcher,
  // IPH). Other callers that only need to record consent use `SetStatus()`.
  virtual void CompleteOnboarding(Profile* profile) const = 0;

  // Applies this FRE's developer switches. Called once per profile at startup,
  // before the status is read.
  virtual void ApplyDevSwitches(Profile* profile) const = 0;

  // Called when the primary account is cleared (Android only).
  virtual void OnPrimaryAccountCleared(Profile* profile) const = 0;

  // Whether the provider's AI settings section stays hidden until this FRE is
  // completed.
  virtual bool HidesSettingsUntilCompleted() const = 0;

  // The profile pref that stores this FRE's status. Observe it to learn when
  // `GetStatus()` may have changed.
  // TODO(b/576047045): Replace with a store-owned observer API once
  // `GlicProviderSpec` is profile-scoped.
  virtual std::string_view GetStatusPrefName() const = 0;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_PUBLIC_GLIC_FRE_STORE_H_
