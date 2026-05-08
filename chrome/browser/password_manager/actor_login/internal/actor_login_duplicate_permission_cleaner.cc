// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/actor_login/internal/actor_login_duplicate_permission_cleaner.h"

#include <utility>

#include "base/barrier_closure.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/concurrent_callbacks.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/password_manager/core/browser/actor_login/actor_login_permission_service.h"
#include "components/password_manager/core/browser/password_form_digest.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_store/password_form_converters.h"
#include "content/public/common/content_features.h"

using password_manager::PasswordForm;
using password_manager_util::GetLoginMatchType;

namespace actor_login {

namespace {
bool ShouldSkipPasswordCredential(const PasswordForm& match,
                                  const Credential& credential,
                                  bool federated_exact_match_exists) {
  if (!match.actor_login_approved) {
    return true;
  }

  if (credential.type == CredentialType::kPassword &&
      match.username_value == credential.username &&
      match.signon_realm == credential.signon_realm) {
    return true;
  }

  // Skip password credentials that belong to the same account
  // as the federated credential. This is either an exact match if
  // one exists, or if not, it can be an affiliated match.
  bool is_exact_match =
      (password_manager_util::GetMatchType(match) == GetLoginMatchType::kExact);
  bool is_affiliated_match = (password_manager_util::GetMatchType(match) ==
                              GetLoginMatchType::kAffiliated);
  if (credential.type == CredentialType::kFederated &&
      match.username_value == credential.username) {
    if (federated_exact_match_exists && is_exact_match) {
      return true;
    }

    if (!federated_exact_match_exists && is_affiliated_match) {
      return true;
    }
  }

  if (password_manager_util::IsCredentialWeakMatch(match)) {
    return true;
  }

  return false;
}
}  // namespace

ActorLoginDuplicatePermissionCleaner::ActorLoginDuplicatePermissionCleaner(
    const Credential& credential,
    scoped_refptr<password_manager::PasswordStoreInterface> profile_store,
    scoped_refptr<password_manager::PasswordStoreInterface> account_store,
    ActorLoginPermissionService* permission_service)
    : credential_(credential),
      profile_store_(std::move(profile_store)),
      account_store_(std::move(account_store)),
      permission_service_(permission_service) {}

ActorLoginDuplicatePermissionCleaner::~ActorLoginDuplicatePermissionCleaner() =
    default;

void ActorLoginDuplicatePermissionCleaner::Start(
    base::OnceClosure done_callback) {
  base::UmaHistogramBoolean(
      "PasswordManager.ActorLogin.DuplicatePermissionCleaner.Invocations",
      true);
  done_callback_ = std::move(done_callback);

  base::RepeatingClosure overall_barrier =
      base::BarrierClosure(2, std::move(done_callback_));
  passwords_done_callback_ = overall_barrier;
  federated_done_callback_ = overall_barrier;

  password_manager::PasswordFormDigest digest(
      password_manager::PasswordForm::Scheme::kHtml,
      credential_.request_origin.GetURL().spec(),
      credential_.request_origin.GetURL());

  pending_password_fetches_ = 0;
  if (profile_store_) {
    profile_store_->GetLogins(digest, weak_ptr_factory_.GetWeakPtr());
    pending_password_fetches_++;
  }
  if (account_store_) {
    account_store_->GetLogins(digest, weak_ptr_factory_.GetWeakPtr());
    pending_password_fetches_++;
  }
  if (pending_password_fetches_ == 0) {
    passwords_done_callback_.Run();
  }

  if (base::FeatureList::IsEnabled(features::kFedCmEmbedderInitiatedLogin)) {
    permission_service_->ListPermissions(
        credential_.request_origin,
        base::BindOnce(
            &ActorLoginDuplicatePermissionCleaner::OnFederatedPermissionsListed,
            weak_ptr_factory_.GetWeakPtr()));
  } else {
    federated_done_callback_.Run();
  }
}

void ActorLoginDuplicatePermissionCleaner::OnGetPasswordStoreResultsOrErrorFrom(
    password_manager::PasswordStoreInterface* store,
    password_manager::LoginsResultOrError results_or_error) {
  if (std::holds_alternative<password_manager::LoginsResult>(
          results_or_error)) {
    const auto& results =
        std::get<password_manager::LoginsResult>(results_or_error);
    for (const auto& match : results) {
      pending_matches_.push_back(password_manager::ToPasswordForm(match));
    }
  }

  pending_password_fetches_--;
  if (pending_password_fetches_ == 0) {
    ClearPasswordPermissions();
  }
}

void ActorLoginDuplicatePermissionCleaner::ClearPasswordPermissions() {
  std::vector<password_manager::StoredCredential> account_updates;
  std::vector<password_manager::StoredCredential> profile_updates;

  bool federated_exact_match_exists =
      credential_.type == CredentialType::kFederated &&
      std::find_if(pending_matches_.begin(), pending_matches_.end(),
                   [this](const PasswordForm& match) {
                     return match.username_value == credential_.username &&
                            password_manager_util::GetMatchType(match) ==
                                GetLoginMatchType::kExact;
                   }) != pending_matches_.end();
  for (const PasswordForm& match : pending_matches_) {
    if (ShouldSkipPasswordCredential(match, credential_,
                                     federated_exact_match_exists)) {
      continue;
    }
    PasswordForm updated_form = match;
    updated_form.actor_login_approved = false;
    if (updated_form.IsUsingAccountStore()) {
      account_updates.push_back(
          password_manager::FromPasswordForm(std::move(updated_form)));
    } else {
      profile_updates.push_back(
          password_manager::FromPasswordForm(std::move(updated_form)));
    }
  }

  base::UmaHistogramCounts100(
      "PasswordManager.ActorLogin.DuplicatePermissionCleaner.PasswordsDeleted",
      account_updates.size() + profile_updates.size());

  size_t pending_updates =
      (account_updates.empty() ? 0 : 1) + (profile_updates.empty() ? 0 : 1);
  if (pending_updates == 0) {
    passwords_done_callback_.Run();
    return;
  }

  base::RepeatingClosure barrier =
      base::BarrierClosure(pending_updates, passwords_done_callback_);

  if (!account_updates.empty()) {
    account_store_->UpdateLogins(
        std::move(account_updates),
        base::BindOnce([](base::RepeatingClosure closure) { closure.Run(); },
                       barrier));
  }

  if (!profile_updates.empty()) {
    profile_store_->UpdateLogins(
        std::move(profile_updates),
        base::BindOnce([](base::RepeatingClosure closure) { closure.Run(); },
                       barrier));
  }
}

void ActorLoginDuplicatePermissionCleaner::OnFederatedPermissionsListed(
    std::vector<FederatedPermission> permissions) {
  CHECK(federated_done_callback_);

  base::ConcurrentCallbacks<bool> concurrent;
  int deleted_count = 0;
  for (const auto& permission : permissions) {
    if (credential_.type == CredentialType::kFederated &&
        permission.MatchesFederatedCredential(credential_)) {
      continue;
    }
    // The permission given for the password credential corresponding to
    // the same account should also not be removed.
    if (credential_.type == CredentialType::kPassword &&
        base::UTF8ToUTF16(permission.chosen_account_email) ==
            credential_.username) {
      continue;
    }
    permission_service_->DeletePermission(credential_.request_origin,
                                          permission.chosen_account_email,
                                          concurrent.CreateCallback());
    deleted_count++;
  }

  base::UmaHistogramCounts100(
      "PasswordManager.ActorLogin.DuplicatePermissionCleaner.FederatedDeleted",
      deleted_count);

  std::move(concurrent)
      .Done(base::IgnoreArgs<std::vector<bool>>(federated_done_callback_));
}

}  // namespace actor_login
