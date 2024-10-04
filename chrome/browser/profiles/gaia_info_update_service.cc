// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/gaia_info_update_service.h"

#include <stddef.h>

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/containers/to_vector.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/avatar_icon_util.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_utils.h"
#include "content/public/browser/storage_partition.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"

namespace {

void UpdateAccountsPrefs(
    PrefService& pref_service,
    const signin::IdentityManager& identity_manager,
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info) {
  if (!accounts_in_cookie_jar_info.AreAccountsFresh()) {
    return;
  }

  base::flat_set<std::string> account_ids_in_chrome =
      signin::GetAllGaiaIdsForKeyedPreferences(&identity_manager,
                                               accounts_in_cookie_jar_info);

  // TODO(b/331767195): In case the prefs are needed for ChromeOS and Android
  // (platforms where the account is tied to the OS) in the future, we would
  // also need to keep the accounts that have an AccountInfo that is still
  // present in Chrome (accounts that have refresh tokens) in addition to the
  // above checks on cookies and primary account.

  SigninPrefs signin_prefs(pref_service);
  size_t removed_count = signin_prefs.RemoveAllAccountPrefsExcept(
      // Convert `std::string` to `SigninPrefs::GaiaId`.
      base::ToVector(account_ids_in_chrome,
                     [](const std::string& gaia_id) -> SigninPrefs::GaiaId {
                       return gaia_id;
                     }));

  if (removed_count > 0) {
    // There is a maximum of 10 Gaia accounts on the web. If we add the Chrome
    // primary account, the theoretical maximum count of accounts in the pref is
    // 11. The histogram function expects the "exclusive maximum" of 11 + 1.
    constexpr int kExclusiveMaxRemovedCount = 12;
    constexpr char kAccountsRemovedHistogramName[] =
        "Signin.AccountPref.RemovedCount";
    base::UmaHistogramExactLinear(kAccountsRemovedHistogramName, removed_count,
                                  kExclusiveMaxRemovedCount);
    std::string variant_histogram_name =
        identity_manager.HasPrimaryAccount(signin::ConsentLevel::kSignin)
            ? base::StrCat({kAccountsRemovedHistogramName, ".SignedIn"})
            : base::StrCat({kAccountsRemovedHistogramName, ".SignedOut"});
    base::UmaHistogramExactLinear(variant_histogram_name, removed_count,
                                  kExclusiveMaxRemovedCount);
  }
}

}  // namespace

GAIAInfoUpdateService::GAIAInfoUpdateService(
    signin::IdentityManager* identity_manager,
    ProfileAttributesStorage* profile_attributes_storage,
    PrefService& pref_service,
    const base::FilePath& profile_path)
    : identity_manager_(identity_manager),
      profile_attributes_storage_(profile_attributes_storage),
      pref_service_(pref_service),
      profile_path_(profile_path) {
  identity_manager_->AddObserver(this);

  if (!ShouldUpdatePrimaryAccount()) {
    ClearProfileEntry();
    return;
  }
  ProfileAttributesEntry* entry =
      profile_attributes_storage_->GetProfileAttributesWithPath(profile_path_);
  if (!entry) {
    return;
  }

  gaia_id_of_profile_attribute_entry_ = entry->GetGAIAId();
}

GAIAInfoUpdateService::~GAIAInfoUpdateService() = default;

void GAIAInfoUpdateService::UpdatePrimaryAccount() {
  if (!ShouldUpdatePrimaryAccount())
    return;

  auto unconsented_primary_account_info =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);

  if (!gaia_id_of_profile_attribute_entry_.empty() &&
      unconsented_primary_account_info.gaia !=
          gaia_id_of_profile_attribute_entry_) {
    ClearProfileEntry();
  }

  UpdatePrimaryAccount(identity_manager_->FindExtendedAccountInfoByAccountId(
      unconsented_primary_account_info.account_id));
}

void GAIAInfoUpdateService::UpdatePrimaryAccount(const AccountInfo& info) {
  if (!info.IsValid())
    return;

  ProfileAttributesEntry* entry =
      profile_attributes_storage_->GetProfileAttributesWithPath(profile_path_);
  if (!entry) {
    return;
  }
  gaia_id_of_profile_attribute_entry_ = info.gaia;
  entry->SetGAIAGivenName(base::UTF8ToUTF16(info.given_name));
  entry->SetGAIAName(base::UTF8ToUTF16(info.full_name));
  entry->SetHostedDomain(info.hosted_domain);

  if (info.picture_url == kNoPictureURLFound) {
    entry->SetGAIAPicture(std::string(), gfx::Image());
  } else if (!info.account_image.IsEmpty()) {
    // Only set the image if it is not empty, to avoid clearing the image if we
    // fail to download it on one of the 24 hours interval to refresh the data.
    entry->SetGAIAPicture(info.last_downloaded_image_url_with_size,
                          info.account_image);
  }
}

void GAIAInfoUpdateService::UpdateAnyAccount(const AccountInfo& info) {
  if (!info.IsValid())
    return;

  ProfileAttributesEntry* entry =
      profile_attributes_storage_->GetProfileAttributesWithPath(profile_path_);
  if (!entry) {
    return;
  }

  // This is idempotent, i.e. the second and any further call for the same
  // account info has no further impact.
  entry->AddAccountName(info.full_name);
}

void GAIAInfoUpdateService::ClearProfileEntry() {
  ProfileAttributesEntry* entry =
      profile_attributes_storage_->GetProfileAttributesWithPath(profile_path_);
  if (!entry) {
    return;
  }
  gaia_id_of_profile_attribute_entry_ = "";
  entry->SetGAIAName(std::u16string());
  entry->SetGAIAGivenName(std::u16string());
  entry->SetGAIAPicture(std::string(), gfx::Image());
  entry->SetHostedDomain(std::string());
}

void GAIAInfoUpdateService::Shutdown() {
  identity_manager_->RemoveObserver(this);
}

void GAIAInfoUpdateService::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  switch (event.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kSet:
      UpdatePrimaryAccount();
      break;
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      ClearProfileEntry();

      // When clearing the primary account, if the account is already removed
      // from the cookie jar, we should remove the prefs as well.
      UpdateAccountsPrefs(pref_service_.get(), *identity_manager_,
                          identity_manager_->GetAccountsInCookieJar());
      break;
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      break;
  }
}

void GAIAInfoUpdateService::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  UpdateAnyAccount(info);

  if (!ShouldUpdatePrimaryAccount())
    return;

  CoreAccountInfo account_info =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);

  if (info.account_id != account_info.account_id)
    return;

  UpdatePrimaryAccount(info);
}

void GAIAInfoUpdateService::OnAccountsInCookieUpdated(
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
  ProfileAttributesEntry* entry =
      profile_attributes_storage_->GetProfileAttributesWithPath(profile_path_);
  if (!entry) {
    return;
  }

  // We can fully regenerate the info about all accounts only when there are no
  // signed-out accounts. This means that for instance clearing cookies will
  // reset the info.
  if (accounts_in_cookie_jar_info.GetSignedOutAccounts().empty()) {
    entry->ClearAccountNames();

    // Regenerate based on the info from signed-in accounts (if not available
    // now, it will be regenerated soon via OnExtendedAccountInfoUpdated() once
    // downloaded).
    for (gaia::ListedAccount account :
         accounts_in_cookie_jar_info.GetPotentiallyInvalidSignedInAccounts()) {
      UpdateAnyAccount(
          identity_manager_->FindExtendedAccountInfoByAccountId(account.id));
    }
  }

  UpdateAccountsPrefs(pref_service_.get(), *identity_manager_,
                      accounts_in_cookie_jar_info);
}

bool GAIAInfoUpdateService::ShouldUpdatePrimaryAccount() {
  return identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin);
}
