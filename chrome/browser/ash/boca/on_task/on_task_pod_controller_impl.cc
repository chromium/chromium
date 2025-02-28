// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/on_task/on_task_pod_controller_impl.h"

#include <memory>
#include <string>

#include "ash/boca/on_task/on_task_pod_utils.h"
#include "ash/boca/on_task/on_task_pod_view.h"
#include "ash/shell.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "chromeos/ui/frame/frame_header.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// Internal name for the OnTask pod widget. Useful for debugging purposes.
constexpr char kOnTaskPodWidgetInternalName[] = "OnTaskPod";

// Creates a child widget for the specified parent window with some common
// characteristics.
std::unique_ptr<views::Widget> CreateChildWidget(
    aura::Window* parent_window,
    const std::string& widget_name,
    std::unique_ptr<views::View> view) {
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.parent = parent_window;
  params.name = widget_name;
  params.activatable = views::Widget::InitParams::Activatable::kDefault;
  params.opacity = views::Widget::InitParams::WindowOpacity::kOpaque;
  auto widget = std::make_unique<views::Widget>();
  widget->Init(std::move(params));
  widget->SetContentsView(std::move(view));
  widget->SetVisibilityAnimationTransition(views::Widget::ANIMATE_NONE);

  return widget;
}
}  // namespace

OnTaskPodControllerImpl::OnTaskPodControllerImpl(Browser* browser)
    : browser_(browser->AsWeakPtr()) {
  aura::Window* const browser_window = browser_->window()->GetNativeWindow();
  auto on_task_pod_view = std::make_unique<OnTaskPodView>(this);
  pod_widget_ = CreateChildWidget(browser_window->GetToplevelWindow(),
                                  kOnTaskPodWidgetInternalName,
                                  std::move(on_task_pod_view));
  pod_widget_->widget_delegate()->SetAccessibleTitle(
      l10n_util::GetStringUTF16(IDS_ON_TASK_POD_ACCESSIBLE_NAME));
  pod_widget_->SetBounds(CalculateWidgetBounds());
  pod_widget_->Show();

  browser_window->AddObserver(this);
}

OnTaskPodControllerImpl::~OnTaskPodControllerImpl() {
  if (browser_) {
    browser_->window()->GetNativeWindow()->RemoveObserver(this);
  }
}

void OnTaskPodControllerImpl::ReloadCurrentPage() {
  if (!browser_) {
    return;
  }
  chrome::Reload(browser_.get(), WindowOpenDisposition::CURRENT_TAB);
}

void OnTaskPodControllerImpl::SetSnapLocation(
    OnTaskPodSnapLocation snap_location) {
  pod_snap_location_ = snap_location;

  // Reposition the widget.
  pod_widget_->SetBounds(CalculateWidgetBounds());
}

void OnTaskPodControllerImpl::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  pod_widget_->SetBounds(CalculateWidgetBounds());
}

const gfx::Rect OnTaskPodControllerImpl::CalculateWidgetBounds() {
  const gfx::Rect parent_window_bounds =
      pod_widget_->parent()->GetWindowBoundsInScreen();
  const gfx::Size preferred_size =
      pod_widget_->GetContentsView()->GetPreferredSize();
  const int frame_header_height =
      boca::GetFrameHeaderHeight(pod_widget_->parent());
  gfx::Point origin;
  switch (pod_snap_location_) {
    case ash::OnTaskPodSnapLocation::kTopLeft:
      origin = gfx::Point(parent_window_bounds.x(),
                          parent_window_bounds.y() + frame_header_height);
      break;
    case ash::OnTaskPodSnapLocation::kTopRight:
      origin = gfx::Point(parent_window_bounds.right() - preferred_size.width(),
                          parent_window_bounds.y() + frame_header_height);
  }

  return gfx::Rect(origin, preferred_size);
}

views::Widget* OnTaskPodControllerImpl::GetPodWidgetForTesting() {
  if (!pod_widget_) {
    return nullptr;
  }
  return pod_widget_.get();
}

OnTaskPodSnapLocation OnTaskPodControllerImpl::GetSnapLocationForTesting()
    const {
  return pod_snap_location_;
}

}  // namespace ash
