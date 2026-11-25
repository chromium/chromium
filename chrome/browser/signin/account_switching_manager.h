// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_ACCOUNT_SWITCHING_MANAGER_H_
#define CHROME_BROWSER_SIGNIN_ACCOUNT_SWITCHING_MANAGER_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/core/browser/account_preview_data_service.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "google_apis/gaia/gaia_id.h"

static_assert(BUILDFLAG(ENABLE_DICE_SUPPORT));

class PrefRegistrySimple;
class PrefService;
class SigninClient;

namespace signin {

class IdentityManager;

// Manages Desktop Smart Account Switching promo eligibility, throttling, and
// account-switching execution for a single regular profile (controlled by
// `switches::kEnableAccountPreviewSwitchingAccount`, which covers both
// in-profile switching and creating a new profile — experiment arms 1 and 2).
class AccountSwitchingManager : public KeyedService {
 public:
  static constexpr base::TimeDelta kPrimaryAccountSigninGracePeriod =
      base::Days(28);
  static constexpr int kMaxGlobalIgnores = 5;
  static constexpr base::TimeDelta kGlobalIgnoreCooldown = base::Days(7);
  static constexpr base::TimeDelta kGlobalExplicitDismissalCooldown =
      base::Days(28);
  static constexpr int kMaxPairExplicitDismissals = 2;

  static bool IsFeatureEnabled();
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // `prefs`, `signin_client`, and `identity_manager` must be non-null and
  // outlive `this`. `account_preview_data_service` may be null (e.g. in tests
  // where `AccountPreviewDataServiceFactory` is not instantiated) and must
  // outlive `this` if non-null.
  AccountSwitchingManager(
      PrefService* prefs,
      SigninClient* signin_client,
      IdentityManager* identity_manager,
      AccountPreviewDataService* account_preview_data_service);

  AccountSwitchingManager(const AccountSwitchingManager&) = delete;
  AccountSwitchingManager& operator=(const AccountSwitchingManager&) = delete;

  ~AccountSwitchingManager() override;

  // KeyedService:
  void Shutdown() override;

  // Returns the current switching candidate from
  // `AccountPreviewDataService::GetPreferredAccountForSwitching()` if it is
  // still fresh at prompt time, or `std::nullopt` otherwise. Validates target
  // account freshness against `IdentityManager` (loaded refresh tokens, valid
  // non-error candidate refresh token, non-managed, non-child) and
  // `SigninClient::IsAccountSignedInOnOtherProfile()` (filtering out stale
  // candidates that have since been signed in on another profile). Promo
  // throttling and primary-account/switch guards are enforced separately by
  // `CanShowPromo()` and `StartSwitching()`.
  std::optional<AccountPreviewDataService::AccountPreviewPreference>
  GetEligibleAccountForSwitching() const;

  // Returns whether offering a Smart Account Switching promo from
  // `primary_gaia_id` to `target_gaia_id` is currently allowed. Checks
  // feature enablement, sign-in allowed pref, account validity, whether
  // `target_gaia_id` is already signed in as primary on another profile,
  // whether a switch is in progress or a switch-back window is active, and
  // (unless the skip-cooldown param is enabled) the sign-in grace period,
  // global ignore/dismissal limits, and bi-directional per-pair suppression in
  // `SigninPrefs`.
  bool CanShowPromo(const GaiaId& primary_gaia_id,
                    const GaiaId& target_gaia_id) const;

  void RecordPromoIgnored();
  // Records an explicit dismissal, updating the global cooldown and
  // incrementing the bi-directional per-pair count in `SigninPrefs`.
  void RecordPromoExplicitlyDismissed(const GaiaId& primary_gaia_id,
                                      const GaiaId& target_gaia_id);

  // Attempts to start switching from `primary_gaia_id` to `target_gaia_id`.
  // Returns true and runs `on_switch_finished` (if non-null) once switching
  // completes or aborts, or drops `on_switch_finished` without running it if
  // `AccountSwitchingManager` is shut down or destroyed first (e.g. on profile
  // shutdown). When switching completes successfully, records bi-directional
  // per-pair suppression for `{primary_gaia_id, target_gaia_id}` in
  // `SigninPrefs`. Returns false without running `on_switch_finished` if
  // switching is disabled by feature flags, sign-in is disallowed, accounts are
  // invalid, `target_gaia_id` is already signed in on another profile, a
  // switch is already in progress, or a switch-back window is active;
  // promo-display throttling does not block `StartSwitching()`.
  bool StartSwitching(
      const GaiaId& primary_gaia_id,
      const GaiaId& target_gaia_id,
      base::OnceClosure on_switch_finished = base::NullCallback());

  void CompleteSwitchForTesting(bool success);

 private:
  struct ActiveSwitch {
    GaiaId primary_gaia_id;
    GaiaId target_gaia_id;
    base::OnceClosure on_finished_callback;
  };

  bool CanStartSwitching(const GaiaId& primary_gaia_id,
                         const GaiaId& target_gaia_id) const;
  bool IsSwitchBackWindowActive() const;
  void OnSwitchFinished(bool success);

  const raw_ref<PrefService> prefs_;
  const raw_ref<SigninClient> signin_client_;
  const raw_ref<IdentityManager> identity_manager_;
  const raw_ptr<AccountPreviewDataService> account_preview_data_service_;
  std::optional<ActiveSwitch> active_switch_;
};

}  // namespace signin

#endif  // CHROME_BROWSER_SIGNIN_ACCOUNT_SWITCHING_MANAGER_H_
