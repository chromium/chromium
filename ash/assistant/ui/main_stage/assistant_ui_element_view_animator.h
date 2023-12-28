// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_UI_ELEMENT_VIEW_ANIMATOR_H_
#define ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_UI_ELEMENT_VIEW_ANIMATOR_H_

#include "ash/assistant/ui/main_stage/element_animator.h"
#include "ash/assistant/util/animation_util.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"

namespace ui {
class CallbackLayerAnimationObserver;
class Layer;
}  // namespace ui

namespace ash {

class AssistantUiElementView;

class AssistantUiElementViewAnimator : public ElementAnimator {
 public:
  using AnimationSmoothnessCallback =
      assistant::util::AnimationSmoothnessCallback;

  AssistantUiElementViewAnimator(AssistantUiElementView* view,
                                 const char* animation_smoothness_histogram);

  explicit AssistantUiElementViewAnimator(
      AssistantUiElementViewAnimator& copy) = delete;
  AssistantUiElementViewAnimator& operator=(
      AssistantUiElementViewAnimator& assign) = delete;
  ~AssistantUiElementViewAnimator() override = default;

  // ElementAnimator:
  void AnimateIn(ui::CallbackLayerAnimationObserver* observer) override;
  void AnimateOut(ui::CallbackLayerAnimationObserver* observer) override;
  ui::Layer* layer() const override;

  AnimationSmoothnessCallback GetAnimationSmoothnessCallback() const;

 private:
  const raw_ptr<AssistantUiElementView> view_;
  std::string const animation_smoothness_histogram_;
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_UI_ELEMENT_VIEW_ANIMATOR_H_
