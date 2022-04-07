// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/quick_start_screen.h"

#include "base/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/oobe_quick_start/verification_shapes.h"
#include "chrome/browser/ui/webui/chromeos/login/quick_start_screen_handler.h"

namespace ash {

// static
std::string QuickStartScreen::GetResultString(Result result) {
  switch (result) {
    case Result::CANCEL:
      return "Cancel";
  }
}

QuickStartScreen::QuickStartScreen(QuickStartView* view,
                                   const ScreenExitCallback& exit_callback)
    : BaseScreen(QuickStartView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(view),
      exit_callback_(exit_callback) {
  if (view_)
    view_->Bind(this);
}

QuickStartScreen::~QuickStartScreen() {
  if (view_)
    view_->Unbind();
}

void QuickStartScreen::OnViewDestroyed(TView* view) {
  if (view_ == view)
    view_ = nullptr;
}

bool QuickStartScreen::MaybeSkip(WizardContext* context) {
  return false;
}

void QuickStartScreen::ShowImpl() {
  if (view_) {
    view_->Show();
  }
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&QuickStartScreen::SendRandomFiguresForTesting,  // IN-TEST
                     base::Unretained(this)),
      base::Seconds(1));
}

void QuickStartScreen::HideImpl() {}

void QuickStartScreen::OnUserActionDeprecated(const std::string& action_id) {
  SendRandomFiguresForTesting();  // IN-TEST
  BaseScreen::OnUserActionDeprecated(action_id);
}

void QuickStartScreen::SendRandomFiguresForTesting() const {
  std::string token = base::UTF16ToASCII(
      base::TimeFormatWithPattern(base::Time::Now(), "MMMMdjmmss"));
  const auto& shapes = quick_start::GenerateShapes(token);
  view_->SetShapes(shapes);
}

}  // namespace ash
