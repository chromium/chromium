// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/quick_unlock_private/quick_unlock_private_ash_utils.h"

#include <utility>

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/login/quick_unlock/auth_token.h"
#include "chrome/browser/ash/login/quick_unlock/fingerprint_storage.h"
#include "chrome/browser/ash/login/quick_unlock/pin_storage_prefs.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_storage.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/quick_unlock_private.h"
#include "chromeos/ash/components/cryptohome/constants.h"
#include "chromeos/ash/components/login/auth/auth_performer.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"
#include "components/user_manager/known_user.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {

using AuthToken = ash::quick_unlock::AuthToken;
using TokenInfo = api::quick_unlock_private::TokenInfo;
using QuickUnlockStorage = ash::quick_unlock::QuickUnlockStorage;

QuickUnlockPrivateGetAuthTokenHelper::QuickUnlockPrivateGetAuthTokenHelper(
    Profile* profile,
    std::string password)
    : profile_(profile),
      password_(std::move(password)),
      auth_performer_(ash::UserDataAuthClient::Get()),
      auth_factor_editor_(ash::UserDataAuthClient::Get()) {}

QuickUnlockPrivateGetAuthTokenHelper::~QuickUnlockPrivateGetAuthTokenHelper() =
    default;

void QuickUnlockPrivateGetAuthTokenHelper::Run(Callback callback) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&QuickUnlockPrivateGetAuthTokenHelper::RunOnUIThread,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void QuickUnlockPrivateGetAuthTokenHelper::RunOnUIThread(Callback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const user_manager::User* const user =
      ash::ProfileHelper::Get()->GetUserByProfile(profile_);
  auto user_context = std::make_unique<ash::UserContext>(*user);

  const bool is_ephemeral =
      ash::ProfileHelper::IsEphemeralUserProfile(profile_);

  auto on_auth_started = base::BindOnce(
      &QuickUnlockPrivateGetAuthTokenHelper::OnAuthSessionStarted,
      weak_factory_.GetWeakPtr(), std::move(callback));

  auth_performer_.StartAuthSession(
      std::move(user_context), is_ephemeral /*ephemeral*/,
      ash::AuthSessionIntent::kDecrypt, std::move(on_auth_started));
}

void QuickUnlockPrivateGetAuthTokenHelper::OnAuthSessionStarted(
    Callback callback,
    bool user_exists,
    std::unique_ptr<ash::UserContext> user_context,
    std::optional<ash::AuthenticationError> error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(user_exists);
  if (error.has_value()) {
    LOG(ERROR) << "Failed to start auth session, code "
               << error->get_cryptohome_error();
    std::move(callback).Run(std::nullopt, *error);
    return;
  }

  const auto* password_factor =
      user_context->GetAuthFactorsData().FindFactorByType(
          cryptohome::AuthFactorType::kPassword);
  if (!password_factor) {
    LOG(ERROR) << "Could not find password key";
    std::move(callback).Run(
        std::nullopt, ash::AuthenticationError(
                          cryptohome::ErrorWrapper::CreateFromErrorCodeOnly(
                              user_data_auth::CRYPTOHOME_ERROR_KEY_NOT_FOUND)));
    return;
  }

  auto on_authenticated =
      base::BindOnce(&QuickUnlockPrivateGetAuthTokenHelper::OnAuthenticated,
                     weak_factory_.GetWeakPtr(), std::move(callback));

  auth_performer_.AuthenticateWithPassword(
      *(password_factor->ref().label()), std::move(password_),
      std::move(user_context), std::move(on_authenticated));
}

void QuickUnlockPrivateGetAuthTokenHelper::OnAuthenticated(
    Callback callback,
    std::unique_ptr<ash::UserContext> user_context,
    std::optional<ash::AuthenticationError> error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (error.has_value()) {
    LOG(ERROR) << "Failed to authenticate with password, code "
               << error->get_cryptohome_error();
    std::move(callback).Run(std::nullopt, *error);
    return;
  }

  auto on_auth_factors_configuration = base::BindOnce(
      &QuickUnlockPrivateGetAuthTokenHelper::OnAuthFactorsConfiguration,
      weak_factory_.GetWeakPtr(), std::move(callback));

  auth_factor_editor_.GetAuthFactorsConfiguration(
      std::move(user_context), std::move(on_auth_factors_configuration));
}

void QuickUnlockPrivateGetAuthTokenHelper::OnAuthFactorsConfiguration(
    Callback callback,
    std::unique_ptr<ash::UserContext> user_context,
    std::optional<ash::AuthenticationError> error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (error.has_value()) {
    LOG(ERROR) << "Failed to load auth factors configuration, code "
               << error->get_cryptohome_error();
    std::move(callback).Run(std::nullopt, *error);
    return;
  }

  // The user context stored in quick_unlock storage must have a device ID, so
  // we retrieve and set it here.
  user_manager::KnownUser known_user{g_browser_process->local_state()};
  std::string device_id = known_user.GetDeviceId(user_context->GetAccountId());
  LOG_IF(WARNING, device_id.empty())
      << "Missing DeviceID for auth factor edits";
  user_context->SetDeviceId(std::move(device_id));

  QuickUnlockStorage* quick_unlock_storage =
      ash::quick_unlock::QuickUnlockFactory::GetForProfile(profile_);
  quick_unlock_storage->MarkStrongAuth();
  // The user has successfully authenticated, so we should reset pin/fingerprint
  // attempt counts.
  quick_unlock_storage->pin_storage_prefs()->ResetUnlockAttemptCount();
  quick_unlock_storage->fingerprint_storage()->ResetUnlockAttemptCount();

  TokenInfo token_info;
  token_info.token =
      ash::AuthSessionStorage::Get()->Store(std::move(user_context));
  token_info.lifetime_seconds =
      cryptohome::kAuthsessionInitialLifetime.InSeconds();

  std::move(callback).Run(std::move(token_info), std::nullopt);
}

}  // namespace extensions
