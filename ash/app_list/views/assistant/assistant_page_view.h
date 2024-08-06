// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_ASSISTANT_ASSISTANT_PAGE_VIEW_H_
#define ASH_APP_LIST_VIEWS_ASSISTANT_ASSISTANT_PAGE_VIEW_H_

#include <memory>
#include <optional>

#include "ash/app_list/views/app_list_page.h"
#include "ash/ash_export.h"
#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/assistant/controller/assistant_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_controller_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/display/display_observer.h"
#include "ui/display/screen.h"

namespace display {
enum class TableState;
}  // namespace display

namespace views {
class ViewShadow;
}  // namespace views

namespace ash {

class AssistantMainView;
class AssistantViewDelegate;

// The Assistant page for the app list.
class ASH_EXPORT AssistantPageView : public AppListPage,
                                     public AssistantControllerObserver,
                                     public AssistantUiModelObserver,
                                     public display::DisplayObserver {
  METADATA_HEADER(AssistantPageView, AppListPage)

 public:
  explicit AssistantPageView(AssistantViewDelegate* assistant_view_delegate);
  AssistantPageView(const AssistantPageView&) = delete;
  AssistantPageView& operator=(const AssistantPageView&) = delete;
  ~AssistantPageView() override;

  // AppListPage:
  gfx::Size GetMinimumSize() const override;
  void OnBoundsChanged(const gfx::Rect& prev_bounds) override;
  void RequestFocus() override;
  void ChildPreferredSizeChanged(views::View* child) override;
  void VisibilityChanged(views::View* starting_from, bool is_visible) override;
  void OnAnimationStarted(AppListState from_state,
                          AppListState to_state) override;
  gfx::Size GetPreferredSearchBoxSize() const override;
  void UpdatePageOpacityForState(AppListState state,
                                 float search_box_opacity) override;
  gfx::Rect GetPageBoundsForState(
      AppListState state,
      const gfx::Rect& contents_bounds,
      const gfx::Rect& search_box_bounds) const override;

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // AssistantControllerObserver:
  void OnAssistantControllerDestroying() override;

  // AssistantUiModelObserver:
  void OnUiVisibilityChanged(
      AssistantVisibility new_visibility,
      AssistantVisibility old_visibility,
      std::optional<AssistantEntryPoint> entry_point,
      std::optional<AssistantExitPoint> exit_point) override;

  // display::DisplayObserver:
  void OnDisplayTabletStateChanged(display::TabletState state) override;

  // views::View:
  void OnThemeChanged() override;

 private:
  void InitLayout();
  void UpdateBackground(bool in_tablet_mode);

  const raw_ptr<AssistantViewDelegate> assistant_view_delegate_;

  // Owned by the view hierarchy.
  raw_ptr<AssistantMainView> assistant_main_view_ = nullptr;

  int min_height_dip_;

  std::unique_ptr<views::ViewShadow> view_shadow_;

  base::ScopedObservation<AssistantController, AssistantControllerObserver>
      assistant_controller_observation_{this};
  base::ScopedObservation<display::Screen, display::DisplayObserver>
      display_observation_{this};
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_ASSISTANT_ASSISTANT_PAGE_VIEW_H_
