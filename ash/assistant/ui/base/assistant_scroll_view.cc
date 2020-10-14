// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/base/assistant_scroll_view.h"

#include <memory>
#include <utility>

#include "base/optional.h"
#include "ui/views/controls/scrollbar/overlay_scroll_bar.h"
#include "ui/views/metadata/metadata_impl_macros.h"

namespace ash {

namespace {

// ContentView ----------------------------------------------------------------

class ContentView : public views::View, views::ViewObserver {
 public:
  ContentView() { AddObserver(this); }

  ContentView(const ContentView&) = delete;
  ContentView& operator=(const ContentView&) = delete;

  ~ContentView() override { RemoveObserver(this); }

  // views::View:
  void ChildPreferredSizeChanged(views::View* child) override {
    PreferredSizeChanged();
  }

  void ChildVisibilityChanged(views::View* child) override {
    PreferredSizeChanged();
  }

  // views::ViewObserver:
  void OnChildViewAdded(views::View* view, views::View* child) override {
    PreferredSizeChanged();
  }

  void OnChildViewRemoved(views::View* view, views::View* child) override {
    PreferredSizeChanged();
  }
};

// InvisibleScrollBar ----------------------------------------------------------

class InvisibleScrollBar : public views::OverlayScrollBar {
 public:
  InvisibleScrollBar(
      base::ObserverList<AssistantScrollView::Observer>* observers,
      bool horizontal)
      : views::OverlayScrollBar(horizontal), observers_(observers) {}

  InvisibleScrollBar(const InvisibleScrollBar&) = delete;
  InvisibleScrollBar& operator=(const InvisibleScrollBar&) = delete;
  ~InvisibleScrollBar() override = default;

  // views::OverlayScrollBar:
  int GetThickness() const override { return 0; }

  void Update(int viewport_size,
              int content_size,
              int content_scroll_offset) override {
    views::OverlayScrollBar::Update(viewport_size, content_size,
                                    content_scroll_offset);
    for (auto& observer : *observers_) {
      observer.OnScrollBarUpdated(this, viewport_size, content_size,
                                  content_scroll_offset);
    }
  }

  void VisibilityChanged(views::View* starting_from, bool is_visible) override {
    if (starting_from != this)
      return;

    for (auto& observer : *observers_)
      observer.OnScrollBarVisibilityChanged(this, is_visible);
  }

 private:
  base::ObserverList<AssistantScrollView::Observer>* observers_;
};

}  // namespace

// AssistantScrollView ---------------------------------------------------------

AssistantScrollView::AssistantScrollView() {
  InitLayout();
}

AssistantScrollView::~AssistantScrollView() = default;

void AssistantScrollView::OnViewPreferredSizeChanged(views::View* view) {
  DCHECK_EQ(content_view_, view);

  for (auto& observer : observers_)
    observer.OnContentsPreferredSizeChanged(content_view_);

  PreferredSizeChanged();
}

void AssistantScrollView::AddScrollViewObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AssistantScrollView::RemoveScrollViewObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void AssistantScrollView::InitLayout() {
  SetBackgroundColor(base::nullopt);
  SetDrawOverflowIndicator(false);

  // Content view.
  auto content_view = std::make_unique<ContentView>();
  content_view->AddObserver(this);
  content_view_ = SetContents(std::move(content_view));

  // Scroll bars.
  horizontal_scroll_bar_ = SetHorizontalScrollBar(
      std::make_unique<InvisibleScrollBar>(&observers_, /*horizontal=*/true));

  vertical_scroll_bar_ = SetVerticalScrollBar(
      std::make_unique<InvisibleScrollBar>(&observers_, /*horizontal=*/false));
}

BEGIN_METADATA(AssistantScrollView, views::ScrollView)
END_METADATA

}  // namespace ash
