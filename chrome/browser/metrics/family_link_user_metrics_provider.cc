// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/family_link_user_metrics_provider.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/session_manager/core/session_manager.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/gaia/oauth2_id_token_decoder.h"

namespace {

constexpr char kHistogramName[] = "ChromeOS.FamilyLinkUser.LogSegment";

}  // namespace

FamilyLinkUserMetricsProvider::FamilyLinkUserMetricsProvider() {
  session_manager::SessionManager* session_manager =
      session_manager::SessionManager::Get();
  // The |session_manager| is nullptr only for unit tests.
  if (session_manager)
    session_manager->AddObserver(this);
}

FamilyLinkUserMetricsProvider::~FamilyLinkUserMetricsProvider() {
  session_manager::SessionManager* session_manager =
      session_manager::SessionManager::Get();
  // The |session_manager| is nullptr only for unit tests.
  if (session_manager)
    session_manager->RemoveObserver(this);
}

// This function is called at unpredictable intervals throughout the entire
// ChromeOS session, so guarantee it will never crash.
void FamilyLinkUserMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto_unused) {
  if (!log_segment_)
    return;
  base::UmaHistogramEnumeration(kHistogramName, log_segment_.value());
}

void FamilyLinkUserMetricsProvider::OnUserSessionStarted(bool is_primary_user) {
  if (!is_primary_user)
    return;

  const user_manager::User* primary_user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  DCHECK(primary_user);
  if (!primary_user->IsChild()) {
    SetLogSegment(LogSegment::kOther);
    return;
  }

  DCHECK(primary_user->is_profile_created());
  Profile* profile =
      chromeos::ProfileHelper::Get()->GetProfileByUser(primary_user);
  DCHECK(profile);
  DCHECK(chromeos::ProfileHelper::IsRegularProfile(profile));

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  DCHECK(identity_manager);

  DCHECK(!access_token_fetcher_);
  access_token_fetcher_ =
      std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
          /*consumer_name=*/"FamilyLinkUserMetricsProvider", identity_manager,
          signin::ScopeSet(),
          base::BindOnce(
              &FamilyLinkUserMetricsProvider::OnAccessTokenRequestCompleted,
              // It is safe to use base::Unretained as |this| owns
              // |access_token_fetcher_|. See comments in
              // primary_account_access_token_fetcher.h.
              base::Unretained(this)),
          signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate,
          signin::ConsentLevel::kSignin);
}

// static
const char* FamilyLinkUserMetricsProvider::GetHistogramNameForTesting() {
  return kHistogramName;
}

void FamilyLinkUserMetricsProvider::SetLogSegment(LogSegment log_segment) {
  log_segment_ = log_segment;
}

void FamilyLinkUserMetricsProvider::OnAccessTokenRequestCompleted(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  access_token_fetcher_.reset();

  if (error.state() != GoogleServiceAuthError::NONE)
    return;

  gaia::TokenServiceFlags service_flags =
      gaia::ParseServiceFlags(access_token_info.id_token);
  LogSegment log_segment = service_flags.is_child_account
                               ? LogSegment::kUnderConsentAge
                               : LogSegment::kOverConsentAge;
  SetLogSegment(log_segment);
}
