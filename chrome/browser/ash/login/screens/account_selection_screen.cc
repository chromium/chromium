// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/account_selection_screen.h"

#include "ash/constants/ash_features.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ui/webui/ash/login/account_selection_screen_handler.h"

namespace ash {
namespace {}  // namespace

// static
std::string AccountSelectionScreen::GetResultString(Result result) {
  // LINT.IfChange(UsageMetrics)
  switch (result) {
    case Result::kGaiaFallback:
      return "GaiaFallback";
    case Result::kNotApplicable:
      return BaseScreen::kNotApplicable;
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)
}

AccountSelectionScreen::AccountSelectionScreen(
    base::WeakPtr<AccountSelectionScreenView> view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(AccountSelectionScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

AccountSelectionScreen::~AccountSelectionScreen() = default;

bool AccountSelectionScreen::MaybeSkip(WizardContext& context) {
  return false;
}

void AccountSelectionScreen::ShowImpl() {
  if (!view_) {
    return;
  }
  view_->Show();
}

void AccountSelectionScreen::HideImpl() {}

void AccountSelectionScreen::OnUserAction(const base::Value::List& args) {
  BaseScreen::OnUserAction(args);
}

}  // namespace ash
