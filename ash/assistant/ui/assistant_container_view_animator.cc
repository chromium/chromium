// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/assistant_container_view_animator.h"

#include "ash/assistant/ui/assistant_container_view.h"
#include "ash/assistant/ui/assistant_container_view_animator_legacy_impl.h"
#include "ash/assistant/ui/assistant_view_delegate.h"

namespace ash {

AssistantContainerViewAnimator::AssistantContainerViewAnimator(
    AssistantViewDelegate* delegate,
    AssistantContainerView* assistant_container_view)
    : delegate_(delegate), assistant_container_view_(assistant_container_view) {
  static_cast<views::View*>(assistant_container_view_)->AddObserver(this);
  delegate_->AddUiModelObserver(this);
}

AssistantContainerViewAnimator::~AssistantContainerViewAnimator() {
  delegate_->RemoveUiModelObserver(this);
  static_cast<views::View*>(assistant_container_view_)->RemoveObserver(this);
}

// static
std::unique_ptr<AssistantContainerViewAnimator>
AssistantContainerViewAnimator::Create(
    AssistantViewDelegate* delegate,
    AssistantContainerView* assistant_container_view) {
  // TODO(wutao): Conditionally provide an alternative implementation.
  return std::make_unique<AssistantContainerViewAnimatorLegacyImpl>(
      delegate, assistant_container_view);
}

void AssistantContainerViewAnimator::Init() {}

void AssistantContainerViewAnimator::OnUiVisibilityChanged(
    AssistantVisibility new_visibility,
    AssistantVisibility old_visibility,
    base::Optional<AssistantEntryPoint> entry_point,
    base::Optional<AssistantExitPoint> exit_point) {}

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
