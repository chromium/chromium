// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/assistant_container_view_animator.h"

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/ui/assistant_container_view.h"
#include "ash/assistant/ui/assistant_container_view_animator_legacy_impl.h"

namespace ash {

AssistantContainerViewAnimator::AssistantContainerViewAnimator(
    AssistantController* assistant_controller,
    AssistantContainerView* assistant_container_view)
    : assistant_controller_(assistant_controller),
      assistant_container_view_(assistant_container_view) {
  static_cast<views::View*>(assistant_container_view_)->AddObserver(this);
}

AssistantContainerViewAnimator::~AssistantContainerViewAnimator() {
  static_cast<views::View*>(assistant_container_view_)->RemoveObserver(this);
}

// static
std::unique_ptr<AssistantContainerViewAnimator>
AssistantContainerViewAnimator::Create(
    AssistantController* assistant_controller,
    AssistantContainerView* assistant_container_view) {
  // TODO(wutao): Conditionally provide an alternative implementation.
  return std::make_unique<AssistantContainerViewAnimatorLegacyImpl>(
      assistant_controller, assistant_container_view);
}

void AssistantContainerViewAnimator::Init() {}

void AssistantContainerViewAnimator::OnBoundsChanged() {}

void AssistantContainerViewAnimator::OnPreferredSizeChanged() {
  if (assistant_container_view_->GetWidget())
    assistant_container_view_->SizeToContents();
}

void AssistantContainerViewAnimator::OnViewBoundsChanged(views::View* view) {
  OnBoundsChanged();
}

void AssistantContainerViewAnimator::OnViewPreferredSizeChanged(
    views::View* view) {
  if (!assistant_container_view_->GetWidget())
    return;

  const gfx::Size preferred_size =
      assistant_container_view_->GetPreferredSize();

  // We currently over-trigger OnViewPreferredSizeChanged. Until that can be
  // addressed we filter out events that are superfluous.
  if (preferred_size == last_preferred_size_)
    return;

  last_preferred_size_ = preferred_size;
  OnPreferredSizeChanged();
}

}  // namespace ash
