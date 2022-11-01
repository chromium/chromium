// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/wrong_hwid_screen.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/ash/login/wrong_hwid_screen_handler.h"

namespace ash {
namespace {

constexpr char kUserActionSkip[] = "skip-screen";

}  // namespace

WrongHWIDScreen::WrongHWIDScreen(base::WeakPtr<WrongHWIDScreenView> view,
                                 const base::RepeatingClosure& exit_callback)
    : BaseScreen(WrongHWIDScreenView::kScreenId,
                 OobeScreenPriority::SCREEN_HARDWARE_ERROR),
      view_(std::move(view)),
      exit_callback_(exit_callback) {
  DCHECK(view_);
}

WrongHWIDScreen::~WrongHWIDScreen() = default;

void WrongHWIDScreen::OnExit() {
  if (is_hidden())
    return;
  exit_callback_.Run();
}

void WrongHWIDScreen::ShowImpl() {
  if (view_)
    view_->Show();
}

void WrongHWIDScreen::HideImpl() {}

void WrongHWIDScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();

  if (action_id == kUserActionSkip) {
    OnExit();
    return;
  }
  BaseScreen::OnUserAction(args);
}

}  // namespace ash
