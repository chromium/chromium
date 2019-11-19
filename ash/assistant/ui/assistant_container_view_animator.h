// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_ASSISTANT_CONTAINER_VIEW_ANIMATOR_H_
#define ASH_ASSISTANT_UI_ASSISTANT_CONTAINER_VIEW_ANIMATOR_H_

#include <memory>

#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/view_observer.h"

namespace views {
class View;
}  // namespace views

namespace ash {

class AssistantContainerView;
class AssistantViewDelegate;

// The AssistantContainerViewAnimator is the class responsible for smoothly
// animating bound changes for the AssistantContainerView.
class COMPONENT_EXPORT(ASSISTANT_UI) AssistantContainerViewAnimator
    : public views::ViewObserver,
      public AssistantUiModelObserver {
 public:
  ~AssistantContainerViewAnimator() override;

  // Returns a newly created instance of an AssistantContainerViewAnimator.
  static std::unique_ptr<AssistantContainerViewAnimator> Create(
      AssistantViewDelegate* delegate,
      AssistantContainerView* assistant_container_view);

  // Invoked when AssistantContainerView has been fully constructed to give the
  // AssistantContainerViewAnimator an opportunity to perform initialization.
  virtual void Init();

  // AssistantUiModelObserver:
  void OnUiVisibilityChanged(
      AssistantVisibility new_visibility,
      AssistantVisibility old_visibility,
      base::Optional<AssistantEntryPoint> entry_point,
      base::Optional<AssistantExitPoint> exit_point) override;

 protected:
  AssistantContainerViewAnimator(
      AssistantViewDelegate* delegate,
      AssistantContainerView* assistant_container_view);

  // Invoked when AssistantContainerView's bounds have changed.
  virtual void OnBoundsChanged();

  // Invoked when AssistantContainerView's preferred size has changed.
  virtual void OnPreferredSizeChanged();

  AssistantViewDelegate* const delegate_;

  // Owned by view hierarchy.
  AssistantContainerView* const assistant_container_view_;

 private:
  // views::Observer:
  void OnViewBoundsChanged(views::View* view) override;
  void OnViewPreferredSizeChanged(views::View* view) override;

  // Cached value of AssistantContainerView's last preferred size. We currently
  // over-trigger the OnViewPreferredSizeChanged event so this is used to filter
  // out superfluous calls.
  gfx::Size last_preferred_size_;

  DISALLOW_COPY_AND_ASSIGN(AssistantContainerViewAnimator);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_ASSISTANT_CONTAINER_VIEW_ANIMATOR_H_
