// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/user_allowlist_check_screen.h"

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "chrome/browser/ui/webui/ash/login/user_allowlist_check_screen_handler.h"
namespace ash {
namespace {

constexpr char kUserActionRetry[] = "retry";

}  // namespace

// static
std::string UserAllowlistCheckScreen::GetResultString(Result result) {
  switch (result) {
    case Result::RETRY:
      return "Retry";
  }
}

UserAllowlistCheckScreen::UserAllowlistCheckScreen(
    base::WeakPtr<UserAllowlistCheckScreenView> view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(UserAllowlistCheckScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

UserAllowlistCheckScreen::~UserAllowlistCheckScreen() = default;

void UserAllowlistCheckScreen::ShowImpl() {
  if (!view_) {
    return;
  }

  const bool enterprise_managed = g_browser_process->platform_part()
                                      ->browser_policy_connector_ash()
                                      ->IsDeviceEnterpriseManaged();

  bool family_link_allowed = false;
  CrosSettings::Get()->GetBoolean(kAccountsPrefFamilyLinkAccountsAllowed,
                                  &family_link_allowed);

  view_->Show(enterprise_managed, family_link_allowed);
}

void UserAllowlistCheckScreen::HideImpl() {
  view_->Hide();
}

void UserAllowlistCheckScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionRetry) {
    exit_callback_.Run(Result::RETRY);
  } else {
    BaseScreen::OnUserAction(args);
  }
}

}  // namespace ash
