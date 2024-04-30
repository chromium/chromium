// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/osauth/osauth_error_screen.h"

#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/screens/osauth/base_osauth_setup_screen.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ui/webui/ash/login/osauth/osauth_error_screen_handler.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/login/auth/public/auth_factors_configuration.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"
#include "chromeos/ash/services/auth_factor_config/auth_factor_config_utils.h"

namespace ash {
namespace {

constexpr const char kUserActionCanel[] = "cancelLoginFlow";

}  // namespace

// static
std::string OSAuthErrorScreen::GetResultString(Result result) {
  // LINT.IfChange(UsageMetrics)
  switch (result) {
    case Result::kAbortSignin:
      return "AbortSignin";
    case Result::kFallbackOnline:
      return "FallbackOnline";
    case Result::kFallbackLocal:
      return "FallbackLocal";
    case Result::kProceedAuthenticated:
      return "ProceedAuthenticated";
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)
}

OSAuthErrorScreen::OSAuthErrorScreen(base::WeakPtr<OSAuthErrorScreenView> view,
                                     ScreenExitCallback exit_callback)
    : BaseOSAuthSetupScreen(OSAuthErrorScreenView::kScreenId,
                            OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(std::move(exit_callback)) {}

OSAuthErrorScreen::~OSAuthErrorScreen() = default;

void OSAuthErrorScreen::ShowImpl() {
  if (!view_) {
    return;
  }
  CHECK(context()->osauth_error.has_value());
  if (context()->osauth_error.value() ==
      WizardContext::OSAuthErrorKind::kRecoveryRotationFailed) {
    // We don't have UI strings now, so just exit.
    exit_callback_.Run(Result::kProceedAuthenticated);
    return;
  }
  if (context()->osauth_error.value() ==
      WizardContext::OSAuthErrorKind::kRecoveryAuthenticationFailed) {
    // We don't have UI strings now, so just pick right factor and exit.
    CHECK(context()->user_context);
    CHECK(context()->user_context->HasAuthFactorsConfiguration());
    const auto& auth_config =
        context()->user_context->GetAuthFactorsConfiguration();

    bool has_online_password = false;
    if (auth_config.HasConfiguredFactor(
            cryptohome::AuthFactorType::kPassword)) {
      has_online_password = auth::IsGaiaPassword(
          *auth_config.FindFactorByType(cryptohome::AuthFactorType::kPassword));
    }
    if (has_online_password) {
      exit_callback_.Run(Result::kFallbackOnline);
    } else {
      exit_callback_.Run(Result::kFallbackLocal);
    }
    return;
  }
  CHECK_EQ(context()->osauth_error.value(),
           WizardContext::OSAuthErrorKind::kFatal);
  view_->Show();
}

void OSAuthErrorScreen::OnUserAction(const base::Value::List& args) {
  CHECK_GE(args.size(), 1u);
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionCanel) {
    if (context()->user_context) {
      context()->user_context.reset();
    }
    if (context()->extra_factors_token.has_value()) {
      AuthSessionStorage::Get()->Invalidate(
          GetToken(), base::BindOnce(&OSAuthErrorScreen::OnTokenInvalidated,
                                     weak_ptr_factory_.GetWeakPtr()));
      return;
    }
    exit_callback_.Run(Result::kAbortSignin);
    return;
  }
  BaseScreen::OnUserAction(args);
}

void OSAuthErrorScreen::OnTokenInvalidated() {
  context()->extra_factors_token = std::nullopt;
  exit_callback_.Run(Result::kAbortSignin);
}

}  // namespace ash
