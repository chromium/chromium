// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_TEXT_ELEMENT_VIEW_H_
#define ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_TEXT_ELEMENT_VIEW_H_

#include <string>

#include "ash/assistant/ui/main_stage/assistant_ui_element_view.h"
#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class Label;
}  // namespace views

namespace ash {

class AssistantTextElement;
class ElementAnimator;

// AssistantTextElementView is the visual representation of an
// AssistantTextElement. It is a child view of UiElementContainerView.
class COMPONENT_EXPORT(ASSISTANT_UI) AssistantTextElementView
    : public AssistantUiElementView {
  METADATA_HEADER(AssistantTextElementView, AssistantUiElementView)

 public:
  explicit AssistantTextElementView(const AssistantTextElement* text_element);

  explicit AssistantTextElementView(const std::string& text);

  AssistantTextElementView(const AssistantTextElementView&) = delete;
  AssistantTextElementView& operator=(const AssistantTextElementView&) = delete;

  ~AssistantTextElementView() override;

  // AssistantUiElementView:
  ui::Layer* GetLayerForAnimating() override;
  std::string ToStringForTesting() const override;
  void ChildPreferredSizeChanged(views::View* child) override;
  std::unique_ptr<ElementAnimator> CreateAnimator() override;

  // views:View:
  void OnThemeChanged() override;

 private:
  void InitLayout(const std::string& text);

  raw_ptr<views::Label> label_;  // Owned by view hierarchy.
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_TEXT_ELEMENT_VIEW_H_
