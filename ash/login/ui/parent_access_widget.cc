// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/parent_access_widget.h"

#include <utility>

#include "ash/keyboard/keyboard_controller_impl.h"
#include "ash/login/ui/parent_access_view.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wm/window_dimmer.h"
#include "components/session_manager/session_manager_types.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

ParentAccessWidget* instance_ = nullptr;

}  // namespace

ParentAccessWidget::TestApi::TestApi(ParentAccessWidget* widget)
    : parent_access_widget_(widget) {}

ParentAccessWidget::TestApi::~TestApi() = default;

ParentAccessView* ParentAccessWidget::TestApi::parent_access_view() {
  return static_cast<ParentAccessView*>(
      parent_access_widget_->widget_->widget_delegate());
}

void ParentAccessWidget::TestApi::SimulateValidationFinished(
    bool access_granted) {
  parent_access_widget_->OnExit(access_granted);
}

// static
void ParentAccessWidget::Show(const AccountId& child_account_id,
                              OnExitCallback callback,
                              ParentAccessRequestReason reason,
                              bool extra_dimmer,
                              base::Time validation_time) {
  if (instance_) {
    VLOG(1) << "Showing existing instance of ParentAccessWidget.";
    instance_->Show();
    return;
  }

  instance_ = new ParentAccessWidget(child_account_id, std::move(callback),
                                     reason, extra_dimmer, validation_time);
}

// static
void ParentAccessWidget::Show(const AccountId& account_id,
                              OnExitCallback callback,
                              ParentAccessRequestReason reason) {
  Show(account_id, std::move(callback), reason, false, base::Time());
}

// static
ParentAccessWidget* ParentAccessWidget::Get() {
  return instance_;
}

void ParentAccessWidget::Destroy() {
  DCHECK_EQ(instance_, this);
  widget_->Close();

  delete instance_;
  instance_ = nullptr;
}

ParentAccessWidget::ParentAccessWidget(const AccountId& account_id,
                                       OnExitCallback callback,
                                       ParentAccessRequestReason reason,
                                       bool extra_dimmer,
                                       base::Time validation_time)
    : callback_(std::move(callback)) {
  views::Widget::InitParams widget_params;
  // Using window frameless to be able to get focus on the view input fields,
  // which does not work with popup type.
  widget_params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  widget_params.ownership =
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget_params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  widget_params.accept_events = true;

  ShellWindowId parent_window_id =
      Shell::Get()->session_controller()->GetSessionState() ==
              session_manager::SessionState::ACTIVE
          ? ash::kShellWindowId_SystemModalContainer
          : ash::kShellWindowId_LockSystemModalContainer;
  widget_params.parent =
      ash::Shell::GetPrimaryRootWindow()->GetChildById(parent_window_id);

  ParentAccessView::Callbacks callbacks;
  callbacks.on_finished = base::BindRepeating(&ParentAccessWidget::OnExit,
                                              weak_factory_.GetWeakPtr());
  widget_params.delegate =
      new ParentAccessView(account_id, callbacks, reason, validation_time);

  if (extra_dimmer)
    dimmer_ = std::make_unique<WindowDimmer>(widget_params.parent);

  widget_ = std::make_unique<views::Widget>();
  widget_->Init(std::move(widget_params));

  Show();
}

ParentAccessWidget::~ParentAccessWidget() = default;

void ParentAccessWidget::Show() {
  if (dimmer_)
    dimmer_->window()->Show();

  DCHECK(widget_);
  widget_->Show();

  auto* keyboard_controller = Shell::Get()->keyboard_controller();
  if (keyboard_controller && keyboard_controller->IsKeyboardEnabled())
    keyboard_controller->HideKeyboard(HideReason::kSystem);
}

void ParentAccessWidget::OnExit(bool success) {
  callback_.Run(success);
  Destroy();
}

}  // namespace ash
