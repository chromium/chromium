// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/split_modifier_keyboard_info_screen.h"

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ui/webui/ash/login/split_modifier_keyboard_info_screen_handler.h"

namespace ash {
namespace {

constexpr char kUserActionNextButtonClicked[] = "next";

}  // namespace

// static
std::string SplitModifierKeyboardInfoScreen::GetResultString(Result result) {
  switch (result) {
    case Result::kNext:
      return "Next";
    case Result::kNotApplicable:
      return BaseScreen::kNotApplicable;
  }
}

SplitModifierKeyboardInfoScreen::SplitModifierKeyboardInfoScreen(
    base::WeakPtr<SplitModifierKeyboardInfoScreenView> view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(SplitModifierKeyboardInfoScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

SplitModifierKeyboardInfoScreen::~SplitModifierKeyboardInfoScreen() = default;

bool SplitModifierKeyboardInfoScreen::MaybeSkip(WizardContext& context) {
  return true;
}

void SplitModifierKeyboardInfoScreen::ShowImpl() {
  if (!view_) {
    return;
  }

  view_->Show();
}

void SplitModifierKeyboardInfoScreen::HideImpl() {}

void SplitModifierKeyboardInfoScreen::OnUserAction(
    const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionNextButtonClicked) {
    exit_callback_.Run(Result::kNext);
  } else {
    BaseScreen::OnUserAction(args);
  }
}

}  // namespace ash
