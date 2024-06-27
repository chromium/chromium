// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/base/assistant_scroll_view.h"

#include <memory>
#include <optional>
#include <utility>

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/scrollbar/overlay_scroll_bar.h"
#include "ui/views/view.h"

namespace ash {

namespace {

// ContentView ----------------------------------------------------------------

class ContentView : public views::View, views::ViewObserver {
  METADATA_HEADER(ContentView, views::View)

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

BEGIN_METADATA(ContentView)
END_METADATA

}  // namespace

// AssistantScrollView ---------------------------------------------------------

AssistantScrollView::AssistantScrollView() {
  InitLayout();
}

AssistantScrollView::~AssistantScrollView() = default;

void AssistantScrollView::OnViewPreferredSizeChanged(views::View* view) {
  DCHECK_EQ(content_view(), view);

  for (auto& observer : observers_)
    observer.OnContentsPreferredSizeChanged(content_view());

  PreferredSizeChanged();
}

void AssistantScrollView::AddScrollViewObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AssistantScrollView::RemoveScrollViewObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void AssistantScrollView::InitLayout() {
  SetBackgroundColor(std::nullopt);
  SetDrawOverflowIndicator(false);

  // Content view.
  content_view_observation_.Observe(
      SetContents(std::make_unique<ContentView>()));

  // Scroll bars.
  SetVerticalScrollBarMode(views::ScrollView::ScrollBarMode::kHiddenButEnabled);
  SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kHiddenButEnabled);
}

BEGIN_METADATA(AssistantScrollView)
END_METADATA

}  // namespace ash
