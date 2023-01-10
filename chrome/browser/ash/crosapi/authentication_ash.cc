// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/authentication_ash.h"

#include <utility>

#include "ash/constants/ash_features.h"
#include "base/check.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_utils_chromeos.h"
#include "chrome/browser/extensions/api/quick_unlock_private/quick_unlock_private_ash_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/extensions/api/quick_unlock_private.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace crosapi {

using TokenInfo = extensions::api::quick_unlock_private::TokenInfo;

AuthenticationAsh::AuthenticationAsh() = default;
AuthenticationAsh::~AuthenticationAsh() = default;

void AuthenticationAsh::BindReceiver(
    mojo::PendingReceiver<mojom::Authentication> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void AuthenticationAsh::CreateQuickUnlockPrivateTokenInfo(
    const std::string& password,
    CreateQuickUnlockPrivateTokenInfoCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  Profile* profile = ProfileManager::GetActiveUserProfile();

  // `helper` is kept alive by binding it to the result callback.
  auto helper =
      std::make_unique<extensions::QuickUnlockPrivateGetAuthTokenHelper>(
          profile, password);
  auto* helper_ptr = helper.get();
  auto on_result_callback = base::BindOnce(
      &AuthenticationAsh::OnCreateQuickUnlockPrivateTokenInfoResults,
      weak_factory_.GetWeakPtr(), std::move(helper), std::move(callback));
  helper_ptr->Run(std::move(on_result_callback));
}

void AuthenticationAsh::IsOsReauthAllowedForActiveUserProfile(
    base::TimeDelta auth_token_lifetime,
    IsOsReauthAllowedForActiveUserProfileCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  bool allowed = extensions::IsOsReauthAllowedAsh(
      ProfileManager::GetActiveUserProfile(), auth_token_lifetime);
  std::move(callback).Run(allowed);
}

void AuthenticationAsh::OnCreateQuickUnlockPrivateTokenInfoResults(
    std::unique_ptr<extensions::QuickUnlockPrivateGetAuthTokenHelper> helper,
    CreateQuickUnlockPrivateTokenInfoCallback callback,
    absl::optional<TokenInfo> token_info,
    absl::optional<ash::AuthenticationError> error) {
  mojom::CreateQuickUnlockPrivateTokenInfoResultPtr result;
  if (!error.has_value()) {
    DCHECK(token_info.has_value());
    crosapi::mojom::QuickUnlockPrivateTokenInfoPtr out_token_info =
        crosapi::mojom::QuickUnlockPrivateTokenInfo::New();
    out_token_info->token = token_info->token;
    out_token_info->lifetime_seconds = token_info->lifetime_seconds;
    result = mojom::CreateQuickUnlockPrivateTokenInfoResult::NewTokenInfo(
        std::move(out_token_info));
  } else {
    DCHECK(error.has_value());
    result = mojom::CreateQuickUnlockPrivateTokenInfoResult::NewErrorMessage(
        extensions::LegacyQuickUnlockPrivateGetAuthTokenHelper::
            kPasswordIncorrect);
  }
  std::move(callback).Run(std::move(result));
}

}  // namespace crosapi
