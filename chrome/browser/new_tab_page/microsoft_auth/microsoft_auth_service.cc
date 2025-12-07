// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/microsoft_auth/microsoft_auth_service.h"

#include <string>
#include <utility>

#include "base/time/time.h"
#include "chrome/browser/new_tab_page/microsoft_auth/microsoft_auth_service_observer.h"
#include "chrome/browser/ui/webui/ntp_microsoft_auth/ntp_microsoft_auth_untrusted_ui.mojom.h"

MicrosoftAuthService::MicrosoftAuthService() = default;
MicrosoftAuthService::~MicrosoftAuthService() = default;

void MicrosoftAuthService::ClearAuthData() {
  access_token_ = new_tab_page::mojom::AccessToken::New();
  state_ = AuthState::kNone;
  NotifyObservers();
}

std::string MicrosoftAuthService::GetAccessToken() {
  CheckAccessTokenExpiration();
  return access_token_->token;
}

MicrosoftAuthService::AuthState MicrosoftAuthService::GetAuthState() {
  CheckAccessTokenExpiration();
  return state_;
}

void MicrosoftAuthService::SetAuthStateError() {
  state_ = AuthState::kError;
  NotifyObservers();
}

void MicrosoftAuthService::SetAccessToken(
    new_tab_page::mojom::AccessTokenPtr access_token) {
  access_token_ = std::move(access_token);
  state_ = AuthState::kSuccess;
  NotifyObservers();
}

void MicrosoftAuthService::AddObserver(MicrosoftAuthServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void MicrosoftAuthService::RemoveObserver(
    MicrosoftAuthServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

void MicrosoftAuthService::CheckAccessTokenExpiration() {
  // Treat access token as expired 30 seconds early to avoid race condition
  // with network calls.
  if (!access_token_->token.empty() &&
      access_token_->expiration <= base::Time::Now() + base::Seconds(30)) {
    // Reset access_token_ and state_, if access_token_ is expired.
    access_token_ = new_tab_page::mojom::AccessToken::New();
    state_ = AuthState::kNone;
    NotifyObservers();
  }
}

void MicrosoftAuthService::NotifyObservers() {
  for (MicrosoftAuthServiceObserver& observer : observers_) {
    observer.OnAuthStateUpdated();
  }
}
