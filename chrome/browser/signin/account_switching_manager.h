// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_ACCOUNT_SWITCHING_MANAGER_H_
#define CHROME_BROWSER_SIGNIN_ACCOUNT_SWITCHING_MANAGER_H_

#include <optional>

#include "base/auto_reset.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/core/browser/account_preview_data_service.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "google_apis/gaia/gaia_id.h"

static_assert(BUILDFLAG(ENABLE_DICE_SUPPORT));

class BrowserWindowInterface;
class PrefRegistrySimple;
class PrefService;
class SigninClient;

namespace signin {

class IdentityManager;

// Outcome of presenting the Desktop Smart Account Switching prompt bubble.
enum class AccountSwitchingPromoResult {
  // The user clicked the accept button to proceed with account switching.
  kAccepted,
  // The user explicitly declined the promo ("Not now").
  kDeclined,
  // The user closed the promo without an explicit accept/decline choice (e.g.
  // close button, Escape key, or clicking the avatar pill).
  kDismissed,
  // The promo was closed or destroyed without any user interaction (e.g.
  // browser window close, preemption by another bubble, or account
  // invalidation while visible).
  kIgnored,
  // The promo bubble was never displayed (e.g. controller unavailable, or
  // aborted before display while waiting for the avatar anchor).
  kNotDisplayed,
  // The promo bubble was not displayed because the browser window has no
  // visible profile avatar button to anchor to.
  kNotDisplayedNoAvatarButton,
};

// Manages Desktop Smart Account Switching promo eligibility, presentation,
// throttling, and account-switching execution for a single regular profile
// (controlled by `switches::kEnableAccountPreviewSwitchingAccount`, which
// covers both in-profile switching and creating a new profile).
class AccountSwitchingManager : public KeyedService {
 public:
  using ShowBubbleCallback = base::RepeatingCallback<base::ScopedClosureRunner(
      BrowserWindowInterface*,
      const AccountInfo&,
      const AccountInfo&,
      base::OnceCallback<void(AccountSwitchingPromoResult)>)>;

  static constexpr base::TimeDelta kPrimaryAccountSigninGracePeriod =
      base::Days(28);
  static constexpr int kMaxGlobalIgnores = 5;
  static constexpr base::TimeDelta kGlobalIgnoreCooldown = base::Days(7);
  static constexpr base::TimeDelta kGlobalExplicitDismissalCooldown =
      base::Days(28);
  static constexpr base::TimeDelta kPostSwitchCooldown = base::Days(7);
  static constexpr int kMaxPairExplicitDismissals = 2;

  static bool IsFeatureEnabled();
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);
  static base::AutoReset<ShowBubbleCallback*> SetShowBubbleCallbackForTesting(
      ShowBubbleCallback* callback);

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

  // Returns whether offering a Smart Account Switching promo is currently
  // allowed for this profile, accounting for feature enablement, account
  // eligibility, active promo/switch guards, and throttling/suppression rules.
  // `ShowPromo()` re-validates eligibility, presents the promo anchored to
  // `browser_window`, and handles the resulting user action internally. Returns
  // true if the promo was shown and false otherwise.
  bool CanShowPromo() const;
  bool ShowPromo(BrowserWindowInterface* browser_window);

  void CompleteSwitchForTesting(bool success);
  bool IsPromoActiveForTesting() const;
  bool IsSwitchInProgressForTesting() const;

 private:
  struct AccountPair {
    GaiaId primary_gaia_id;
    GaiaId target_gaia_id;

    bool operator==(const AccountPair&) const = default;
  };

  // Promo eligibility checks.
  std::optional<AccountPair> GetEligiblePromoPair(
      bool check_promo_eligibility = true) const;
  std::optional<CoreAccountInfo> GetEligiblePrimaryAccount() const;
  std::optional<AccountPreviewDataService::AccountPreviewPreference>
  GetEligibleAccountForSwitching() const;
  bool IsAccountEligible(const AccountInfo& account_info) const;
  bool CanShowPromoForPair(const AccountPair& pair) const;

  // Promo result handling and throttling state updates.
  void OnPromoResult(AccountSwitchingPromoResult result);
  void RecordPromoIgnored();
  void RecordPromoExplicitlyDismissed(const AccountPair& pair);

  // Account switching checks and execution.
  bool IsProfileEligibleForSwitching() const;
  bool IsSwitchBackWindowActive() const;
  bool StartSwitching(AccountPair pair);
  void OnSwitchFinished(bool success);

  const raw_ref<PrefService> prefs_;
  const raw_ref<SigninClient> signin_client_;
  const raw_ref<IdentityManager> identity_manager_;
  const raw_ptr<AccountPreviewDataService> account_preview_data_service_;

  std::optional<AccountPair> active_promo_;
  std::optional<AccountPair> active_switch_;
  base::ScopedClosureRunner close_promo_bubble_;

  // Declared last so pending promo callbacks are invalidated before
  // `close_promo_bubble_` closes any open bubble on destruction.
  base::WeakPtrFactory<AccountSwitchingManager> weak_ptr_factory_{this};
};

}  // namespace signin

#endif  // CHROME_BROWSER_SIGNIN_ACCOUNT_SWITCHING_MANAGER_H_
