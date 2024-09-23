// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_BASE_ASSISTANT_SCROLL_VIEW_H_
#define ASH_ASSISTANT_UI_BASE_ASSISTANT_SCROLL_VIEW_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/view_observer.h"

namespace ash {

class COMPONENT_EXPORT(ASSISTANT_UI) AssistantScrollView
    : public views::ScrollView,
      public views::ViewObserver {
  METADATA_HEADER(AssistantScrollView, views::ScrollView)

 public:
  class Observer : public base::CheckedObserver {
   public:
    // Invoked when the scrollable contents' preferred size has changed.
    virtual void OnContentsPreferredSizeChanged(views::View* content_view) {}

   protected:
    Observer() = default;
    ~Observer() override = default;
  };

  AssistantScrollView();
  AssistantScrollView(const AssistantScrollView&) = delete;
  AssistantScrollView& operator=(const AssistantScrollView) = delete;
  ~AssistantScrollView() override;

  // views::ViewObserver:
  void OnViewPreferredSizeChanged(views::View* view) override;

  // Adds/removes the specified |observer|.
  void AddScrollViewObserver(Observer* observer);
  void RemoveScrollViewObserver(Observer* observer);

  views::View* content_view() { return content_view_observation_.GetSource(); }
  const views::View* content_view() const {
    return content_view_observation_.GetSource();
  }

 private:
  void InitLayout();

  base::ObserverList<Observer> observers_;

  // The observed view is owned by the view hierarchy. This could be a raw_ptr
  // to the view + ScopedObservation, but accessing the view through the
  // ScopedObservation saves a pointer.
  base::ScopedObservation<views::View, views::ViewObserver>
      content_view_observation_{this};
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_BASE_ASSISTANT_SCROLL_VIEW_H_
