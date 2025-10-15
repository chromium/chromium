// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_PROMO_UTIL_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_PROMO_UTIL_H_

#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
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

class PrefService;

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

// Structure containing information needed for the promos.
struct ProfileMenuAvatarButtonPromoInfo {
  // Different promo types that can be shown in the ProfileMenu and
  // AvatarButton.
  enum class Type {
    kHistorySyncPromo,
    kBatchUploadPromo,
    kBatchUploadBookmarksPromo,
    kBatchUploadWindows10DepreciationPromo,
    kSyncPromo,
  };

  std::optional<Type> type = std::nullopt;
  size_t local_data_count = 0;

  friend bool operator==(const ProfileMenuAvatarButtonPromoInfo& info1,
                         const ProfileMenuAvatarButtonPromoInfo& info2) =
      default;
};

// Based on the `profile` current state, compute the data to be shown for the
// promos, if any, based on the promo priority and the profile state. The promo
// between the ProfileMenu and the AvatarButton should always be aligned.
void ComputeProfileMenuAvatarButtonPromoInfo(
    Profile& profile,
    base::OnceCallback<void(ProfileMenuAvatarButtonPromoInfo)> result_callback);

class SyncPromoIdentityPillManager : public signin::IdentityManager::Observer {
 public:
  explicit SyncPromoIdentityPillManager(
      signin::IdentityManager* identity_manager,
      PrefService* pref_service);
  // Used only for testing.
  SyncPromoIdentityPillManager(signin::IdentityManager* identity_manager,
                               PrefService* pref_service,
                               int max_shown_count,
                               int max_used_count);
  ~SyncPromoIdentityPillManager() override;

  SyncPromoIdentityPillManager(const SyncPromoIdentityPillManager&) = delete;
  SyncPromoIdentityPillManager& operator=(const SyncPromoIdentityPillManager&) =
      delete;

  SyncPromoIdentityPillManager(SyncPromoIdentityPillManager&&) = delete;
  SyncPromoIdentityPillManager& operator=(SyncPromoIdentityPillManager&&) =
      delete;

  bool ShouldShowPromo(ProfileMenuAvatarButtonPromoInfo::Type promo_type);
  void RecordPromoShown(ProfileMenuAvatarButtonPromoInfo::Type promo_type);
  void RecordPromoUsed(ProfileMenuAvatarButtonPromoInfo::Type promo_type);

  // signin::IdentityManager::Observer:
  void OnIdentityManagerShutdown(IdentityManager* identity_manager) override;

 private:
  bool ArePromotionsEnabled() const;
  // Returns an empty account if the profile sign in state is anything different
  // than signed in.
  AccountInfo GetSignedInAccountInfo() const;

  raw_ptr<signin::IdentityManager> identity_manager_;
  // Only nullptr after the `identity_manager_` starts shutting down.
  std::unique_ptr<SigninPrefs> signin_prefs_;

  const int max_shown_count_ = 0;
  const int max_used_count_ = 0;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_scoped_observation_{this};
};
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

}  // namespace signin

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_PROMO_UTIL_H_
