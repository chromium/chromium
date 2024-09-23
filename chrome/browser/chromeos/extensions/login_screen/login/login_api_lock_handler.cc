// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login/login_api_lock_handler.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "chrome/browser/ash/login/lock/screen_locker.h"
#include "chrome/browser/ui/ash/session/session_controller_client_impl.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"

namespace chromeos {

namespace {

LoginApiLockHandler* g_lock_handler_for_testing = nullptr;

}  // namespace

// static
LoginApiLockHandler* LoginApiLockHandler::Get() {
  if (g_lock_handler_for_testing)
    return g_lock_handler_for_testing;
  static base::NoDestructor<LoginApiLockHandler> lock_handler;
  return lock_handler.get();
}

// static
void LoginApiLockHandler::SetInstanceForTesting(LoginApiLockHandler* instance) {
  g_lock_handler_for_testing = instance;
}

LoginApiLockHandler::LoginApiLockHandler() = default;

LoginApiLockHandler::~LoginApiLockHandler() = default;

void LoginApiLockHandler::RequestLockScreen() {
  SessionControllerClientImpl::Get()->RequestLockScreen();
}

void LoginApiLockHandler::Authenticate(
    const ash::UserContext& user_context,
    base::OnceCallback<void(bool auth_success)> callback) {
  unlock_in_progress_ = true;
  callback_ = std::move(callback);
  ash::ScreenLocker::default_screen_locker()->Authenticate(
      std::make_unique<ash::UserContext>(user_context),
      base::BindOnce(&LoginApiLockHandler::AuthenticateCallback,
                     weak_factory_.GetWeakPtr()));
}

void LoginApiLockHandler::AuthenticateCallback(bool auth_success) {
  unlock_in_progress_ = false;
  std::move(callback_).Run(auth_success);
}

bool LoginApiLockHandler::IsUnlockInProgress() const {
  return unlock_in_progress_;
}

}  // namespace chromeos
