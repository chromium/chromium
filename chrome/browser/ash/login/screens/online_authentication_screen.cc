// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/online_authentication_screen.h"

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/ash/login/online_authentication_screen_handler.h"

namespace ash {
namespace {

constexpr char kUserActionBack[] = "back";

}  // namespace

// static
std::string OnlineAuthenticationScreen::GetResultString(Result result) {
  // LINT.IfChange(UsageMetrics)
  switch (result) {
    case Result::BACK:
      return "Back";
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)
}

OnlineAuthenticationScreen::OnlineAuthenticationScreen(
    base::WeakPtr<OnlineAuthenticationScreenView> view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(OnlineAuthenticationScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

OnlineAuthenticationScreen::~OnlineAuthenticationScreen() = default;

void OnlineAuthenticationScreen::ShowImpl() {
  if (!view_) {
    return;
  }

  view_->Show();
}

void OnlineAuthenticationScreen::HideImpl() {
  view_->Hide();
}

void OnlineAuthenticationScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionBack) {
    exit_callback_.Run(Result::BACK);
  } else {
    BaseScreen::OnUserAction(args);
  }
}

}  // namespace ash
