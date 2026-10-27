// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_BROWSER_UI_ACTIVITY_LIST_BUBBLE_GLIC_ACTIVITY_LIST_BUBBLE_H_
#define CHROME_BROWSER_GLIC_BROWSER_UI_ACTIVITY_LIST_BUBBLE_GLIC_ACTIVITY_LIST_BUBBLE_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/glic/browser_ui/activity_list_bubble/glic_activity_row_view_model.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class View;
class Widget;
}  // namespace views

DECLARE_ELEMENT_IDENTIFIER_VALUE(kActorTaskListBubbleView);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kActorTaskListBubbleScrollView);

namespace glic {

// Bubble that displays notifications about the user's ongoing activities and
// tasks.
class GlicActivityListBubble : public views::WidgetObserver {
 public:
  explicit GlicActivityListBubble(
      std::vector<GlicActivityRowViewModel> row_models,
      base::RepeatingClosure on_destroyed = {});
  ~GlicActivityListBubble() override;

  void Show(views::View* anchor_view);
  void Close();
  bool IsShowing() const;

  views::Widget* widget() { return widget_; }

  // views::WidgetObserver:
  void OnWidgetDestroyed(views::Widget* widget) override;

 private:
  std::unique_ptr<views::View> CreateContentsView();

  std::vector<GlicActivityRowViewModel> row_models_;
  base::RepeatingClosure on_destroyed_;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};
  raw_ptr<views::Widget> widget_ = nullptr;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_BROWSER_UI_ACTIVITY_LIST_BUBBLE_GLIC_ACTIVITY_LIST_BUBBLE_H_
