// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_BASE_ASSISTANT_SCROLL_VIEW_H_
#define ASH_ASSISTANT_UI_BASE_ASSISTANT_SCROLL_VIEW_H_

#include "base/component_export.h"
#include "base/observer_list.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/view_observer.h"

namespace ash {

class COMPONENT_EXPORT(ASSISTANT_UI) AssistantScrollView
    : public views::ScrollView,
      public views::ViewObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Invoked when the scrollable contents' preferred size has changed.
    virtual void OnContentsPreferredSizeChanged(views::View* content_view) {}

    // Invoked when the specified |scroll_bar| has been updated.
    virtual void OnScrollBarUpdated(views::ScrollBar* scroll_bar,
                                    int viewport_size,
                                    int content_size,
                                    int content_scroll_offset) {}

    // Invoked when the visibility for the specified |scroll_bar| has changed.
    virtual void OnScrollBarVisibilityChanged(views::ScrollBar* scroll_bar,
                                              bool is_visible) {}

   protected:
    Observer() = default;
    ~Observer() override = default;
  };

  METADATA_HEADER(AssistantScrollView);

  AssistantScrollView();
  AssistantScrollView(const AssistantScrollView&) = delete;
  AssistantScrollView& operator=(const AssistantScrollView) = delete;
  ~AssistantScrollView() override;

  // views::ViewObserver:
  void OnViewPreferredSizeChanged(views::View* view) override;

  // Adds/removes the specified |observer|.
  void AddScrollViewObserver(Observer* observer);
  void RemoveScrollViewObserver(Observer* observer);

  views::View* content_view() { return content_view_; }
  const views::View* content_view() const { return content_view_; }

  views::ScrollBar* horizontal_scroll_bar() { return horizontal_scroll_bar_; }
  views::ScrollBar* vertical_scroll_bar() { return vertical_scroll_bar_; }

 private:
  void InitLayout();

  base::ObserverList<Observer> observers_;

  views::View* content_view_;                // Owned by view hierarchy.
  views::ScrollBar* horizontal_scroll_bar_;  // Owned by view hierarchy.
  views::ScrollBar* vertical_scroll_bar_;    // Owned by view hierarchy.
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_BASE_ASSISTANT_SCROLL_VIEW_H_
