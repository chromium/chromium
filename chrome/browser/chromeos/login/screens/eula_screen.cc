// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/eula_screen.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/customization/customization_document.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/ui/webui/chromeos/login/eula_screen_handler.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/dbus_thread_manager.h"

namespace chromeos {
namespace {

constexpr const char kUserActionAcceptButtonClicked[] = "accept-button";
constexpr const char kUserActionBackButtonClicked[] = "back-button";

// Reflects the value of usage statistics reporting checkbox shown in eula
// UI. The value is expected to survive EulaScreen res-hows within a single
// session. For example if a user unchecks the checkbox, goes back, and then
// gets to EULA screen again, the checkbox should be unchecked.
bool g_usage_statistics_reporting_enabled = true;

}  // namespace

EulaScreen::EulaScreen(EulaView* view, const ScreenExitCallback& exit_callback)
    : BaseScreen(EulaView::kScreenId),
      view_(view),
      exit_callback_(exit_callback),
      password_fetcher_(this) {
  DCHECK(view_);
  if (view_)
    view_->Bind(this);
}

EulaScreen::~EulaScreen() {
  if (view_)
    view_->Unbind();
}

void EulaScreen::InitiatePasswordFetch() {
  if (tpm_password_.empty()) {
    password_fetcher_.Fetch();
    // Will call view after password has been fetched.
  } else if (view_) {
    view_->OnPasswordFetched(tpm_password_);
  }
}

void EulaScreen::SetUsageStatsEnabled(bool enabled) {
  g_usage_statistics_reporting_enabled = enabled;
}

bool EulaScreen::IsUsageStatsEnabled() const {
  return g_usage_statistics_reporting_enabled;
}

void EulaScreen::OnViewDestroyed(EulaView* view) {
  if (view_ == view)
    view_ = NULL;
}

void EulaScreen::Show() {
  // Command to own the TPM.
  CryptohomeClient::Get()->TpmCanAttemptOwnership(
      EmptyVoidDBusMethodCallback());
  if (WizardController::UsingHandsOffEnrollment())
    OnUserAction(kUserActionAcceptButtonClicked);
  else if (view_)
    view_->Show();
}

void EulaScreen::Hide() {
  if (view_)
    view_->Hide();
}

void EulaScreen::OnUserAction(const std::string& action_id) {
  if (action_id == kUserActionAcceptButtonClicked) {
    exit_callback_.Run(g_usage_statistics_reporting_enabled
                           ? Result::ACCEPTED_WITH_USAGE_STATS_REPORTING
                           : Result::ACCEPTED_WITHOUT_USAGE_STATS_REPORTING);
  } else if (action_id == kUserActionBackButtonClicked) {
    exit_callback_.Run(Result::BACK);
  } else {
    BaseScreen::OnUserAction(action_id);
  }
}

void EulaScreen::OnPasswordFetched(const std::string& tpm_password) {
  tpm_password_ = tpm_password;
  if (view_)
    view_->OnPasswordFetched(tpm_password_);
}

}  // namespace chromeos
