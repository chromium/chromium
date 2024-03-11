// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/categories_selection_screen.h"

#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/ash/login/categories_selection_screen_handler.h"

namespace ash {
namespace {

// constexpr const char kUserActionNext[] = "next";
// constexpr const char kUserActionSkip[] = "skip";

}  // namespace

// static
std::string CategoriesSelectionScreen::GetResultString(Result result) {
  switch (result) {
    case Result::kNext:
      return "Next";
    case Result::kSkip:
      return "Skip";
    case Result::kNotApplicable:
      return BaseScreen::kNotApplicable;
  }
}

CategoriesSelectionScreen::CategoriesSelectionScreen(
    base::WeakPtr<CategoriesSelectionScreenView> view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(CategoriesSelectionScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

CategoriesSelectionScreen::~CategoriesSelectionScreen() = default;

bool CategoriesSelectionScreen::MaybeSkip(WizardContext& context) {
  if (context.skip_post_login_screens_for_tests) {
    exit_callback_.Run(Result::kNotApplicable);
    return true;
  }

  return false;
}

void CategoriesSelectionScreen::ShowImpl() {
  if (!view_) {
    return;
  }

  view_->Show();
}

void CategoriesSelectionScreen::HideImpl() {}

void CategoriesSelectionScreen::OnUserAction(const base::Value::List& args) {
  NOTIMPLEMENTED();
}

}  // namespace ash
