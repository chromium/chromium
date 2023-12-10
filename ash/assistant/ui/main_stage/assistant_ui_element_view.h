// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_UI_ELEMENT_VIEW_H_
#define ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_UI_ELEMENT_VIEW_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

class ElementAnimator;

// Base class for a visual representation of an AssistantUiElement. It is a
// child view of UiElementContainerView.
class COMPONENT_EXPORT(ASSISTANT_UI) AssistantUiElementView
    : public views::View {
  METADATA_HEADER(AssistantUiElementView, views::View)

 public:
  explicit AssistantUiElementView(AssistantUiElementView& copy) = delete;
  AssistantUiElementView& operator=(AssistantUiElementView& assign) = delete;
  ~AssistantUiElementView() override;

  // Returns the layer that should be used when animating this view.
  virtual ui::Layer* GetLayerForAnimating() = 0;

  // Returns a string representation of this view for testing.
  virtual std::string ToStringForTesting() const = 0;

  // Returns a newly created animator which is used by UiElementContainerView
  // to animate this view on/off stage in sync with Assistant response events.
  virtual std::unique_ptr<ElementAnimator> CreateAnimator() = 0;

 protected:
  AssistantUiElementView();
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_UI_ELEMENT_VIEW_H_
