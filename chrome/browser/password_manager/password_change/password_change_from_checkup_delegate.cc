// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/password_change_from_checkup_delegate.h"

#include <utility>

#include "chrome/browser/password_manager/password_change/glic_password_change_actuator.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"

PasswordChangeFromCheckupDelegate::PasswordChangeFromCheckupDelegate() =
    default;

PasswordChangeFromCheckupDelegate::~PasswordChangeFromCheckupDelegate() {
  Stop(actor::ActorTask::StoppedReason::kShutdown);
  actuator_.reset();
}

void PasswordChangeFromCheckupDelegate::StartPasswordChangeFlow(
    password_manager::StoredCredential credential,
    base::WeakPtr<content::WebContents> web_contents,
    StateChangeCallback callback) {
  if (!web_contents) {
    return;
  }
  web_contents_ = web_contents;
  state_change_callback_ = std::move(callback);

  if (!actuator_) {
    Profile* profile =
        Profile::FromBrowserContext(web_contents->GetBrowserContext());
    actuator_ = std::make_unique<GlicPasswordChangeActuator>(
        std::move(credential), web_contents.get(), profile);
    actuator_->AddObserver(this);
  }
  actuator_->Start();
}

void PasswordChangeFromCheckupDelegate::Stop(
    actor::ActorTask::StoppedReason stop_reason) {
  if (actuator_) {
    actuator_->RemoveObserver(this);
    actuator_->Cancel();
    actuator_.reset();
  }

  if (state_change_callback_) {
    state_change_callback_.Run(PasswordAutomaticChangeState::kInactive);
    state_change_callback_.Reset();
  }
}

void PasswordChangeFromCheckupDelegate::OnActuationStateChanged(
    PasswordChangeActuator::State new_state) {
  if (!state_change_callback_) {
    return;
  }

  switch (new_state) {
    case PasswordChangeActuator::State::kWaitingForChangePasswordForm:
      state_change_callback_.Run(
          PasswordAutomaticChangeState::kAttemptingSignIn);
      break;
    case PasswordChangeActuator::State::kChangingPassword:
      state_change_callback_.Run(
          PasswordAutomaticChangeState::kChangingPassword);
      break;
    case PasswordChangeActuator::State::kPasswordSuccessfullyChanged:
      state_change_callback_.Run(
          PasswordAutomaticChangeState::kPasswordChangedSuccessfully);
      break;
    case PasswordChangeActuator::State::kPasswordChangeFailed:
    case PasswordChangeActuator::State::kChangePasswordFormNotFound:
    case PasswordChangeActuator::State::kOtpDetected:
      state_change_callback_.Run(PasswordAutomaticChangeState::kError);
      break;
  }
}

void PasswordChangeFromCheckupDelegate::OpenActuationTab() {
  if (actuator_) {
    actuator_->OpenPasswordChangeTab(web_contents_.get());
  }
}

std::u16string PasswordChangeFromCheckupDelegate::generated_password() const {
  return actuator_ ? actuator_->GetGeneratedPassword() : std::u16string();
}
