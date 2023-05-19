// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/toast/anchored_nudge.h"

#include "ash/public/cpp/shelf_types.h"
#include "ash/style/system_toast_style.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "ui/aura/window.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/event.h"
#include "ui/events/event_observer.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/event_monitor.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {

///////////////////////////////////////////////////////////////////////////////
//  HoverObserver
class AnchoredNudge::HoverObserver : public ui::EventObserver {
 public:
  using HoverStateChangeCallback =
      base::RepeatingCallback<void(bool is_hovering)>;

  HoverObserver(aura::Window* widget_window,
                HoverStateChangeCallback on_hover_state_changed)
      : event_monitor_(views::EventMonitor::CreateWindowMonitor(
            /*event_observer=*/this,
            widget_window,
            {ui::ET_MOUSE_ENTERED, ui::ET_MOUSE_EXITED})),
        on_hover_state_changed_(std::move(on_hover_state_changed)) {}

  HoverObserver(const HoverObserver&) = delete;

  HoverObserver& operator=(const HoverObserver&) = delete;

  ~HoverObserver() override = default;

  // ui::EventObserver:
  void OnEvent(const ui::Event& event) override {
    switch (event.type()) {
      case ui::ET_MOUSE_ENTERED:
        on_hover_state_changed_.Run(/*is_hovering=*/true);
        break;
      case ui::ET_MOUSE_EXITED:
        on_hover_state_changed_.Run(/*is_hovering=*/false);
        break;
      default:
        NOTREACHED();
        break;
    }
  }

 private:
  // While this `EventMonitor` object exists, this object will only look for
  // `ui::ET_MOUSE_ENTERED` and `ui::ET_MOUSE_EXITED` events that occur in the
  // `widget_window` indicated in the constructor.
  std::unique_ptr<views::EventMonitor> event_monitor_;

  // This is run whenever the mouse enters or exits the observed window with a
  // parameter to indicate whether the window is being hovered.
  HoverStateChangeCallback on_hover_state_changed_;
};

///////////////////////////////////////////////////////////////////////////////
//  AnchoredNudge
AnchoredNudge::AnchoredNudge(Delegate* delegate,
                             const AnchoredNudgeData& nudge_data)
    : views::BubbleDialogDelegateView(nudge_data.anchor_view,
                                      nudge_data.arrow,
                                      views::BubbleBorder::NO_SHADOW),
      delegate_(delegate),
      id_(nudge_data.id) {
  SetButtons(ui::DIALOG_BUTTON_NONE);
  set_color(SK_ColorTRANSPARENT);
  set_margins(gfx::Insets());
  set_close_on_deactivate(false);
  SetLayoutManager(std::make_unique<views::FlexLayout>());
  toast_contents_view_ = AddChildView(std::make_unique<SystemToastStyle>(
      nudge_data.dismiss_callback, nudge_data.text, nudge_data.dismiss_text));
}

AnchoredNudge::~AnchoredNudge() {
  hover_observer_.reset();
}

const std::u16string& AnchoredNudge::GetText() {
  CHECK(toast_contents_view_);
  CHECK(toast_contents_view_->label());
  return toast_contents_view_->label()->GetText();
}

std::unique_ptr<views::NonClientFrameView>
AnchoredNudge::CreateNonClientFrameView(views::Widget* widget) {
  // Create the customized bubble border.
  std::unique_ptr<views::BubbleBorder> bubble_border =
      std::make_unique<views::BubbleBorder>(arrow(),
                                            views::BubbleBorder::NO_SHADOW);
  bubble_border->set_avoid_shadow_overlap(true);

  // TODO(b/279769899): Have insets adjust to shelf alignment, and set their
  // value from a param in AnchoredNudge constructor. The value 16 works for VC
  // tray icons because the icon is 8px away from the shelf top and we need an
  // extra 8 for spacing between the shelf and nudge.
  bubble_border->set_insets(gfx::Insets(16));

  auto frame = BubbleDialogDelegateView::CreateNonClientFrameView(widget);
  static_cast<views::BubbleFrameView*>(frame.get())
      ->SetBubbleBorder(std::move(bubble_border));
  return frame;
}

void AnchoredNudge::AddHoverObserver(gfx::NativeWindow native_window) {
  hover_observer_ = std::make_unique<HoverObserver>(
      native_window, base::BindRepeating(&AnchoredNudge::OnHoverStateChanged,
                                         base::Unretained(this)));
}

void AnchoredNudge::OnHoverStateChanged(bool is_hovering) {
  CHECK(hover_observer_);
  if (!GetWidget()) {
    return;
  }

  // TODO(b/282805056): Handle hover state observations directly in the manager.
  delegate_->OnNudgeHoverStateChanged(id_, is_hovering);
}

BEGIN_METADATA(AnchoredNudge, views::BubbleDialogDelegateView)
END_METADATA

}  // namespace ash
