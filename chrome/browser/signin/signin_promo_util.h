// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_PROMO_UTIL_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_PROMO_UTIL_H_

#include "base/memory/raw_ref.h"
#include "build/build_config.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "extensions/buildflags/buildflags.h"

class Profile;

namespace signin_metrics {
enum class AccessPoint;
}

namespace autofill {
class AutofillProfile;
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
namespace extensions {
class Extension;
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

namespace signin {

enum class SignInPromoType;

#if !BUILDFLAG(IS_ANDROID)
// Whether we should show the sync promo.
bool ShouldShowSyncPromo(Profile& profile);
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_EXTENSIONS)
// Whether we should show the sync promo after an extension was installed.
bool ShouldShowExtensionSyncPromo(Profile& profile,
                                  const extensions::Extension& extension);

// Whether we should show the sign in promo after an extension was installed.
bool ShouldShowExtensionSignInPromo(Profile& profile,
                                    const extensions::Extension& extension);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

// Whether we should show the sign in promo after a password was saved.
bool ShouldShowPasswordSignInPromo(Profile& profile);

// Whether we should show the sign in promo after `address` was saved.
bool ShouldShowAddressSignInPromo(Profile& profile,
                                  const autofill::AutofillProfile& address);

// Whether we should show the sign in promo after a bookmark was saved.
bool ShouldShowBookmarkSignInPromo(Profile& profile);

// Returns whether `access_point` has an equivalent autofill signin promo.
bool IsAutofillSigninPromo(signin_metrics::AccessPoint access_point);

// Returns whether `access_point` has an equivalent signin promo.
bool IsSignInPromo(signin_metrics::AccessPoint access_point);

SignInPromoType GetSignInPromoTypeFromAccessPoint(
    signin_metrics::AccessPoint access_point);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Records that the sign in promo was shown, either for the account used for the
// promo, or for the profile if there is no account available.
void RecordSignInPromoShown(signin_metrics::AccessPoint access_point,
                            Profile* profile);

class SyncPromoIdentityPillManager {
 public:
  explicit SyncPromoIdentityPillManager(Profile& profile);
  // Used only for testing.
  SyncPromoIdentityPillManager(Profile& profile,
                               int max_shown_count,
                               int max_used_count);

  SyncPromoIdentityPillManager(const SyncPromoIdentityPillManager&) = delete;
  SyncPromoIdentityPillManager& operator=(const SyncPromoIdentityPillManager&) =
      delete;

  SyncPromoIdentityPillManager(SyncPromoIdentityPillManager&&) = delete;
  SyncPromoIdentityPillManager& operator=(SyncPromoIdentityPillManager&&) =
      delete;

  bool ShouldShowPromo() const;
  void RecordPromoShown();
  void RecordPromoUsed();

 private:
  bool ArePromotionsEnabled() const;

  const raw_ref<Profile> profile_;

  const int max_shown_count_ = 0;
  const int max_used_count_ = 0;
};
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

}  // namespace signin

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_PROMO_UTIL_H_
