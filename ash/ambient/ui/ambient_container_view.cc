// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/ambient_container_view.h"

#include <memory>
#include <utility>

#include "ash/ambient/ui/ambient_assistant_container_view.h"
#include "ash/ambient/ui/ambient_view_delegate.h"
#include "ash/ambient/ui/photo_view.h"
#include "ash/ambient/util/ambient_util.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ash/assistant/util/animation_util.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/public/cpp/ambient/ambient_ui_model.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/events/event_observer.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/background.h"
#include "ui/views/event_monitor.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

using chromeos::assistant::features::IsAmbientAssistantEnabled;

// Appearance.
constexpr int kAssistantPreferredHeightDip = 128;

// A tolerance threshold used to ignore spurious mouse move.
constexpr int kMouseMoveErrorTolerancePx = 3;

}  // namespace

// HostWidgetEventObserver----------------------------------

// A pre target event handler installed on the hosting widget of
// |AmbientContainerView| to capture key and mouse events regardless of whether
// |AmbientContainerView| has focus.
class AmbientContainerView::HostWidgetEventObserver : public ui::EventObserver {
 public:
  explicit HostWidgetEventObserver(AmbientContainerView* container)
      : container_(container) {
    DCHECK(container_);
    event_monitor_ = views::EventMonitor::CreateWindowMonitor(
        this, container_->GetWidget()->GetNativeWindow()->GetRootWindow(),
        {ui::ET_KEY_PRESSED, ui::ET_MOUSE_ENTERED, ui::ET_MOUSE_MOVED,
         ui::ET_TOUCH_PRESSED, ui::ET_TOUCH_MOVED});
  }

  ~HostWidgetEventObserver() override = default;

  HostWidgetEventObserver(const HostWidgetEventObserver&) = delete;
  HostWidgetEventObserver& operator=(const HostWidgetEventObserver&) = delete;

  // ui::EventObserver:
  void OnEvent(const ui::Event& event) override {
    switch (event.type()) {
      case ui::ET_KEY_PRESSED:
        DCHECK(event.IsKeyEvent());
        container_->HandleEvent();
        break;
      case ui::ET_TOUCH_PRESSED:
      case ui::ET_TOUCH_MOVED:
        container_->HandleEvent();
        break;
      case ui::ET_MOUSE_ENTERED:
        DCHECK(event.IsMouseEvent());
        // Updates the mouse enter location.
        mouse_enter_location_ = event.AsMouseEvent()->location();
        break;
      case ui::ET_MOUSE_MOVED:
        DCHECK(event.IsMouseEvent());
        if (CountAsRealMove(event.AsMouseEvent()->location()))
          container_->HandleEvent();
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  bool CountAsRealMove(const gfx::Point& new_mouse_location) {
    // We will ignore all tiny moves (when the cursor moves within
    // |kMouseMoveErrorTolerancePlx| on both directions) to avoid being too
    // sensitive to mouse movement. Any mouse moves beyond that are considered
    // as real mouse move events.
    return (abs(new_mouse_location.x() - mouse_enter_location_.x()) >
                kMouseMoveErrorTolerancePx ||
            abs(new_mouse_location.y() - mouse_enter_location_.y()) >
                kMouseMoveErrorTolerancePx);
  }

 private:
  AmbientContainerView* const container_;
  std::unique_ptr<views::EventMonitor> event_monitor_;

  // Tracks the mouse location when entering the control boundary of the host
  // widget.
  gfx::Point mouse_enter_location_;
};

AmbientContainerView::AmbientContainerView(AmbientViewDelegate* delegate)
    : delegate_(delegate) {
  SetID(AssistantViewID::kAmbientContainerView);
  Init();
}

AmbientContainerView::~AmbientContainerView() {
  event_observer_.reset();
}

const char* AmbientContainerView::GetClassName() const {
  return "AmbientContainerView";
}

gfx::Size AmbientContainerView::CalculatePreferredSize() const {
  // TODO(b/139953389): Handle multiple displays.
  return GetWidget()->GetNativeWindow()->GetRootWindow()->bounds().size();
}

void AmbientContainerView::Layout() {
  // Layout child views first to have proper bounds set for children.
  LayoutPhotoView();

  // The assistant view may not exist if |kAmbientAssistant| feature is
  // disabled.
  if (ambient_assistant_container_view_)
    LayoutAssistantView();

  View::Layout();
}

void AmbientContainerView::AddedToWidget() {
  event_observer_ = std::make_unique<HostWidgetEventObserver>(this);
}

void AmbientContainerView::Init() {
  // TODO(b/139954108): Choose a better dark mode theme color.
  SetBackground(views::CreateSolidBackground(SK_ColorBLACK));
  // Updates focus behavior to receive key press events.
  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);

  photo_view_ = AddChildView(std::make_unique<PhotoView>(delegate_));

  if (IsAmbientAssistantEnabled()) {
    ambient_assistant_container_view_ =
        AddChildView(std::make_unique<AmbientAssistantContainerView>());
    ambient_assistant_container_view_->SetVisible(false);
  }
}

void AmbientContainerView::LayoutPhotoView() {
  // |photo_view_| should have the same size as the widget.
  photo_view_->SetBoundsRect(GetLocalBounds());
}

void AmbientContainerView::LayoutAssistantView() {
  int preferred_width = GetPreferredSize().width();
  int preferred_height = kAssistantPreferredHeightDip;
  ambient_assistant_container_view_->SetBoundsRect(
      gfx::Rect(0, 0, preferred_width, preferred_height));
}

void AmbientContainerView::HandleEvent() {
  delegate_->OnBackgroundPhotoEvents();
}

}  // namespace ash
