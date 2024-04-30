// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/user_allowlist_check_screen.h"

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/ash/login/user_allowlist_check_screen_handler.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/settings/cros_settings.h"

namespace ash {
namespace {

constexpr char kUserActionRetry[] = "retry";

}  // namespace

// static
std::string UserAllowlistCheckScreen::GetResultString(Result result) {
  // LINT.IfChange(UsageMetrics)
  switch (result) {
    case Result::RETRY:
      return "Retry";
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)
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

  const bool enterprise_managed =
      ash::InstallAttributes::Get()->IsEnterpriseManaged();

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
