// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/drive/drive_service.h"

#include <memory>
#include <utility>

#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/signin/public/identity_manager/scope_set.h"

namespace {
// The scope required for an access token in order to query ItemSuggest.
constexpr char kDriveScope[] = "https://www.googleapis.com/auth/drive.readonly";
}  // namespace

DriveService::~DriveService() = default;

DriveService::DriveService(signin::IdentityManager* identity_manager)
    : identity_manager_(identity_manager) {}

void DriveService::GetDriveSuggestions(SuggestionsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug/1168763) May need to handle multiple requests after
  // token_fetcher has been set.
  token_fetcher_ = std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
      "ntp_drive_module", identity_manager_, signin::ScopeSet({kDriveScope}),
      base::BindOnce(&DriveService::OnTokenReceived, weak_factory_.GetWeakPtr(),
                     std::move(callback)),
      signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate,
      signin::ConsentLevel::kSync);
}

void DriveService::OnTokenReceived(SuggestionsCallback callback,
                                   GoogleServiceAuthError error,
                                   signin::AccessTokenInfo token_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  token_fetcher_.reset();

  if (error.state() != GoogleServiceAuthError::NONE) {
    std::move(callback).Run("");
    return;
  }
  std::move(callback).Run("valid token");
}
