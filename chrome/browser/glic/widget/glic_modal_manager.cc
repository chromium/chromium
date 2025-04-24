// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/glic_modal_manager.h"

#include "chrome/browser/glic/widget/glic_modal_view.h"

namespace {

constexpr int kHorizontalPadding = 16;
constexpr int kVerticalPadding = 16;
constexpr int kDistanceFromBottom = 16;

}  // namespace

namespace glic {

GlicModalManager::GlicModalManager() = default;

GlicModalManager::~GlicModalManager() = default;

void GlicModalManager::ShowModal(std::u16string label,
                                 views::Widget* glic_widget) {
  if (!glic_widget || modal_widget_) {
    return;
  }
  modal_widget_ = std::make_unique<views::Widget>();

  auto modal_view = std::make_unique<GlicModalView>(
      glic_widget->GetColorProvider(), label,
      base::BindRepeating(&GlicModalManager::CloseModal, base::Unretained(this),
                          views::Widget::ClosedReason::kCancelButtonClicked));

  auto params = views::Widget::InitParams(
      views::Widget::InitParams::Ownership::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::Type::TYPE_POPUP);
  params.parent = glic_widget->GetNativeView();
  modal_widget_->Init(std::move(params));
  modal_widget_->MakeCloseSynchronous(
      base::BindOnce(&GlicModalManager::CloseModal, base::Unretained(this)));
  modal_widget_->SetBounds(GetModalBounds(glic_widget, modal_view.get()));
  modal_widget_->SetContentsView(std::move(modal_view));
  modal_widget_->Show();

  // Automatically hide the dialog after 5 seconds.
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner =
      base::SequencedTaskRunner::GetCurrentDefault();
  ui_task_runner->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&GlicModalManager::CloseModal, base::Unretained(this),
                     views::Widget::ClosedReason::kUnspecified),
      base::Seconds(5));
}

void GlicModalManager::CloseModal(views::Widget::ClosedReason reason) {
  modal_widget_.reset();
}

gfx::Rect GlicModalManager::GetModalBounds(views::Widget* glic_widget,
                                           GlicModalView* modal_view) {
  gfx::Rect parent_bounds = glic_widget->GetWindowBoundsInScreen();

  views::SizeBounds available_size(
      parent_bounds.size() -
      gfx::Size(kHorizontalPadding * 2, kVerticalPadding * 2));
  gfx::Size preferred_size = modal_view->GetPreferredSize(available_size);

  // Center modal over parent horizontally and position it kDistanceFromBottom
  // from the bottom of the parent.
  gfx::Point center_point = parent_bounds.CenterPoint();
  int x = center_point.x() - preferred_size.width() / 2;
  int y =
      parent_bounds.bottom() - kDistanceFromBottom - preferred_size.height();
  return gfx::Rect(x, y, preferred_size.width(), preferred_size.height());
}

}  // namespace glic
