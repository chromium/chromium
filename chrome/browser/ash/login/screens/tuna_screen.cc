// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/tuna_screen.h"

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ui/webui/ash/login/tuna_screen_handler.h"

namespace ash {
namespace {

constexpr char kUserActionBackButtonClicked[] = "back";
constexpr char kUserActionNextButtonClicked[] = "next";

}  // namespace

// static
std::string TunaScreen::GetResultString(Result result) {
  switch (result) {
    case Result::kNext:
      return "Manual";
    case Result::kBack:
      return "Back";
    case Result::kNotApplicable:
      return BaseScreen::kNotApplicable;
  }
}

TunaScreen::TunaScreen(base::WeakPtr<TunaScreenView> view,
                               const ScreenExitCallback& exit_callback)
    : BaseScreen(TunaScreenView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

TunaScreen::~TunaScreen() = default;

bool TunaScreen::MaybeSkip(WizardContext& context) {
  return true;
}

void TunaScreen::ShowImpl() {
  if (!view_) {
    return;
  }

  view_->Show();
}

void TunaScreen::HideImpl() {}

void TunaScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionNextButtonClicked) {
    exit_callback_.Run(Result::kNext);
  } else if (action_id == kUserActionBackButtonClicked) {
    exit_callback_.Run(Result::kBack);
  } else {
    BaseScreen::OnUserAction(args);
  }
}

}  // namespace ash
