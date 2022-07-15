// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/quick_unlock_private/quick_unlock_private_ash_utils.h"

#include <utility>

#include "ash/components/login/auth/extended_authenticator.h"
#include "ash/components/login/auth/public/user_context.h"
#include "base/bind.h"
#include "chrome/browser/ash/login/quick_unlock/auth_token.h"
#include "chrome/browser/ash/login/quick_unlock/fingerprint_storage.h"
#include "chrome/browser/ash/login/quick_unlock/pin_storage_prefs.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_storage.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/quick_unlock_private.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {

using AuthToken = ash::quick_unlock::AuthToken;
using TokenInfo = api::quick_unlock_private::TokenInfo;
using QuickUnlockStorage = ash::quick_unlock::QuickUnlockStorage;

namespace {

const char kPasswordIncorrect[] = "Incorrect Password.";

}  // namespace

/******** QuickUnlockPrivateGetAuthTokenHelper ********/

QuickUnlockPrivateGetAuthTokenHelper::QuickUnlockPrivateGetAuthTokenHelper(
    Profile* profile)
    : profile_(profile) {}

QuickUnlockPrivateGetAuthTokenHelper::~QuickUnlockPrivateGetAuthTokenHelper() =
    default;

void QuickUnlockPrivateGetAuthTokenHelper::Run(
    ash::ExtendedAuthenticator* extended_authenticator,
    const std::string& password,
    QuickUnlockPrivateGetAuthTokenHelper::ResultCallback callback) {
  callback_ = std::move(callback);

  const user_manager::User* const user =
      ash::ProfileHelper::Get()->GetUserByProfile(profile_);
  ash::UserContext user_context(*user);
  user_context.SetKey(ash::Key(password));

  AddRef();

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ash::ExtendedAuthenticator::AuthenticateToCheck,
                     extended_authenticator, user_context,
                     base::OnceClosure()));
}

void QuickUnlockPrivateGetAuthTokenHelper::OnAuthFailure(
    const ash::AuthFailure& error) {
  std::move(callback_).Run(false, nullptr, kPasswordIncorrect);

  Release();  // Balanced in Run().
}

void QuickUnlockPrivateGetAuthTokenHelper::OnAuthSuccess(
    const ash::UserContext& user_context) {
  auto token_info = std::make_unique<TokenInfo>();

  QuickUnlockStorage* quick_unlock_storage =
      ash::quick_unlock::QuickUnlockFactory::GetForProfile(profile_);
  quick_unlock_storage->MarkStrongAuth();
  token_info->token = quick_unlock_storage->CreateAuthToken(user_context);
  token_info->lifetime_seconds = AuthToken::kTokenExpirationSeconds;

  // The user has successfully authenticated, so we should reset pin/fingerprint
  // attempt counts.
  quick_unlock_storage->pin_storage_prefs()->ResetUnlockAttemptCount();
  quick_unlock_storage->fingerprint_storage()->ResetUnlockAttemptCount();

  std::move(callback_).Run(true, std::move(token_info), "");

  Release();  // Balanced in Run().
}

}  // namespace extensions
