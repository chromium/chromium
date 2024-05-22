// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/pin_request_widget.h"

#include <utility>

#include "ash/keyboard/keyboard_controller_impl.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wm/window_dimmer.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "components/session_manager/session_manager_types.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

PinRequestWidget* instance_ = nullptr;

base::RepeatingClosure& GetOnShownCallback() {
  static base::NoDestructor<base::RepeatingClosure> on_shown;
  return *on_shown;
}

}  // namespace

PinRequestWidget::TestApi::TestApi(PinRequestWidget* widget)
    : pin_request_widget_(widget) {}

PinRequestWidget::TestApi::~TestApi() = default;

PinRequestView* PinRequestWidget::TestApi::pin_request_view() {
  return pin_request_widget_->GetView();
}

// static
void PinRequestWidget::Show(PinRequest request,
                            PinRequestView::Delegate* delegate) {
  DCHECK(!instance_);
  instance_ = new PinRequestWidget(std::move(request), delegate);
  if (GetOnShownCallback()) {
    GetOnShownCallback().Run();
  }
}

// static
PinRequestWidget* PinRequestWidget::Get() {
  return instance_;
}

// static
void PinRequestWidget::SetShownCallbackForTesting(
    base::RepeatingClosure on_shown) {
  GetOnShownCallback() = std::move(on_shown);
}

void PinRequestWidget::UpdateState(PinRequestViewState state,
                                   const std::u16string& title,
                                   const std::u16string& description) {
  DCHECK_EQ(instance_, this);
  GetView()->UpdateState(state, title, description);
}

void PinRequestWidget::SetPinInputEnabled(bool enabled) {
  DCHECK_EQ(instance_, this);
  GetView()->SetInputEnabled(enabled);
}

void PinRequestWidget::ClearInput() {
  DCHECK_EQ(instance_, this);
  GetView()->ClearInput();
}

void PinRequestWidget::Close(bool success) {
  DCHECK_EQ(instance_, this);
  PinRequestWidget* instance = instance_;
  instance_ = nullptr;
  std::move(on_pin_request_done_).Run(success);
  widget_->Close();
  delete instance;
}

PinRequestWidget::PinRequestWidget(PinRequest request,
                                   PinRequestView::Delegate* delegate)
    : on_pin_request_done_(std::move(request.on_pin_request_done)) {
  // Using window frameless to be able to get focus on the view input fields,
  // which does not work with popup type.
  views::Widget::InitParams widget_params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  widget_params.opacity =
      views::Widget::InitParams::WindowOpacity::kTranslucent;
  widget_params.accept_events = true;

  ShellWindowId parent_window_id =
      Shell::Get()->session_controller()->GetSessionState() ==
              session_manager::SessionState::ACTIVE
          ? kShellWindowId_SystemModalContainer
          : kShellWindowId_LockSystemModalContainer;
  widget_params.parent =
      Shell::GetPrimaryRootWindow()->GetChildById(parent_window_id);
  if (request.extra_dimmer) {
    dimmer_ = std::make_unique<WindowDimmer>(widget_params.parent);
  }
  request.on_pin_request_done =
      base::BindOnce(&PinRequestWidget::Close, weak_factory_.GetWeakPtr());
  widget_params.delegate = new PinRequestView(std::move(request), delegate);

  widget_ = std::make_unique<views::Widget>();
  widget_->Init(std::move(widget_params));

  Show();
}

PinRequestWidget::~PinRequestWidget() = default;

void PinRequestWidget::Show() {
  if (dimmer_) {
    dimmer_->window()->Show();
  }

  DCHECK(widget_);
  widget_->Show();

  auto* keyboard_controller = Shell::Get()->keyboard_controller();
  if (keyboard_controller && keyboard_controller->IsKeyboardEnabled()) {
    keyboard_controller->HideKeyboard(HideReason::kSystem);
  }
}

PinRequestView* PinRequestWidget::GetView() {
  return static_cast<PinRequestView*>(widget_->widget_delegate());
}

}  // namespace ash
