// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/search_results_panel.h"

#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ash_web_view.h"
#include "ash/public/cpp/ash_web_view_factory.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/display/screen.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/wm/core/shadow_types.h"

namespace ash {

namespace {

// TODO(sophiewen): Remove hardcoded values when we get UX specs.
constexpr int kSearchResultsPanelWidth = 600;

}  // namespace

SearchResultsPanel::SearchResultsPanel() {
  DCHECK(features::IsSunfishFeatureEnabled());
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // TODO(b/358124802): Add `SearchBoxView` child view.

  search_results_view_ =
      AddChildView(CaptureModeController::Get()->CreateSearchResultsView());

  // TODO(b/356878705): Replace this when the backend is hooked up. Currently
  // used for UI debugging.
  search_results_view_->Navigate(GURL("https://www.google.com/search?q=cat"));
}

SearchResultsPanel::~SearchResultsPanel() = default;

// static
std::unique_ptr<views::Widget> SearchResultsPanel::CreateWidget() {
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  // TODO(b/356878705): Use the captured region display and bounds.
  const gfx::Rect work_area(
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area());
  gfx::Rect bounds(work_area.right() - kSearchResultsPanelWidth, work_area.y(),
                   kSearchResultsPanelWidth, work_area.height());
  params.bounds = bounds;
  params.opacity = views::Widget::InitParams::WindowOpacity::kOpaque;
  params.activatable = views::Widget::InitParams::Activatable::kYes;
  params.shadow_elevation = wm::kShadowElevationInactiveWindow;
  auto widget = std::make_unique<views::Widget>(std::move(params));
  widget->SetContentsView(std::make_unique<SearchResultsPanel>());
  return widget;
}

BEGIN_METADATA(SearchResultsPanel)
END_METADATA

}  // namespace ash
