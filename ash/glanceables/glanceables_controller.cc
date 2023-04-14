// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/glanceables_controller.h"

#include <memory>

#include "ash/ambient/ambient_controller.h"
#include "ash/glanceables/glanceables_delegate.h"
#include "ash/glanceables/glanceables_view.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/time/calendar_utils.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/layer.h"
#include "ui/views/background.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/public/activation_client.h"

namespace ash {
namespace {

// Returns true if `window` appears in any desk container on any display.
bool IsWindowOnAnyDesk(aura::Window* window) {
  DCHECK(window);
  for (aura::Window* root : Shell::GetAllRootWindows()) {
    for (aura::Window* desk : desks_util::GetDesksContainers(root)) {
      if (desk->Contains(window)) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace

GlanceablesController::GlanceablesController()
    : start_of_month_utc_(
          calendar_utils::GetStartOfMonthUTC(base::Time::Now())) {
  Shell::Get()->activation_client()->AddObserver(this);
  Shell::Get()->tablet_mode_controller()->AddObserver(this);
}

GlanceablesController::~GlanceablesController() {
  Shell::Get()->tablet_mode_controller()->RemoveObserver(this);
  Shell::Get()->activation_client()->RemoveObserver(this);
}

void GlanceablesController::Init(
    std::unique_ptr<GlanceablesDelegate> delegate) {
  DCHECK(delegate);
  delegate_ = std::move(delegate);
}

void GlanceablesController::ShowOnLogin() {
  // Adding current month to the set of *non-prunable* months will trigger
  // another fetch and `OnEventsFetched` call. Otherwise, the default behavior
  // is that *prunable* months are cached and do not trigger another fetch.
  // TODO(crbug.com/1360403): Move this somewhere else if `ShowOnLogin` won't be
  // a guranteed single entry point for glanceables (ideally in a method of
  // `SessionObserver`).
  Shell::Get()->system_tray_model()->calendar_model()->AddNonPrunableMonth(
      start_of_month_utc_);

  if (Shell::Get()->IsInTabletMode()) {
    // TODO(crbug.com/1360528): Implement tablet mode support.
    return;
  }

  CreateUi();
  FetchData();
}

bool GlanceablesController::IsShowing() const {
  return !!widget_;
}

void GlanceablesController::CreateUi() {
  widget_ = std::make_unique<views::Widget>();
  views::Widget::InitParams params;
  params.delegate = new views::WidgetDelegate;  // Takes ownership.
  params.delegate->SetOwnedByWidget(true);
  // Allow maximize so the glanceable container's FillLayoutManager can fill the
  // screen with the widget. This is required even for fullscreen widgets.
  params.delegate->SetCanMaximize(true);
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.name = "GlanceablesWidget";
  params.show_state = ui::SHOW_STATE_FULLSCREEN;
  // Create the glanceables widget on the primary display.
  params.parent = Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                                      kShellWindowId_GlanceablesContainer);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  widget_->Init(std::move(params));

  view_ = widget_->SetContentsView(std::make_unique<GlanceablesView>());

  ApplyBackdrop();
  widget_->Show();
}

void GlanceablesController::DestroyUi() {
  widget_.reset();
  view_ = nullptr;
  delegate_->OnGlanceablesClosed();
  weather_refresher_.reset();
}

void GlanceablesController::OnWindowActivated(
    wm::ActivationChangeObserver::ActivationReason reason,
    aura::Window* gained_focus,
    aura::Window* lost_focus) {
  if (!gained_focus)
    return;

  // Destroy the UI if the activated window appears on any desk. This includes
  // browser windows, PWA windows, ARC windows, etc.
  if (IsWindowOnAnyDesk(gained_focus) && IsShowing())
    DestroyUi();
}

void GlanceablesController::OnTabletModeStarted() {
  if (!IsShowing())
    return;

  // TODO(crbug.com/1360528): Implement tablet mode support.
  DestroyUi();
}

void GlanceablesController::FetchData() {
  // GlanceablesWeatherView observes the weather model for updates.
  weather_refresher_ = Shell::Get()
                           ->ambient_controller()
                           ->ambient_weather_controller()
                           ->CreateScopedRefresher();

  Shell::Get()->system_tray_model()->calendar_model()->FetchEvents(
      start_of_month_utc_);
}

void GlanceablesController::ApplyBackdrop() const {
  auto* layer = widget_->GetLayer();
  layer->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
  layer->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);
  view_->SetBackground(
      views::CreateSolidBackground(SkColorSetARGB(0x80, 0x00, 0x00, 0x00)));
}

}  // namespace ash
