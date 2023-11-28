// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/osauth/osauth_error_screen.h"

#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/screens/osauth/base_osauth_setup_screen.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ui/webui/ash/login/osauth/osauth_error_screen_handler.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"

namespace ash {
namespace {

constexpr const char kUserActionCanel[] = "cancelLoginFlow";

}  // namespace

// static
std::string OSAuthErrorScreen::GetResultString(Result result) {
  switch (result) {
    case Result::kAbortSignin:
      return "AbortSignin";
  }
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
  view_->Show();
}

void OSAuthErrorScreen::OnUserAction(const base::Value::List& args) {
  CHECK_GE(args.size(), 1u);
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionCanel) {
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
  context()->extra_factors_token = absl::nullopt;
  exit_callback_.Run(Result::kAbortSignin);
}

}  // namespace ash
