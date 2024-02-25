// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/lock_screen_action/lock_screen_action_background_view.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/animation/square_ink_drop_ripple.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

class LockScreenActionBackgroundView::NoteBackground : public views::View {
  METADATA_HEADER(NoteBackground, views::View)

 public:
  explicit NoteBackground(views::InkDropObserver* observer)
      : observer_(observer) {
    DCHECK(observer);
    views::InkDrop::Install(this, std::make_unique<views::InkDropHost>(this));
    views::InkDrop::Get(this)->SetMode(
        views::InkDropHost::InkDropMode::ON_NO_GESTURE_HANDLER);
    views::InkDrop::Get(this)->SetCreateInkDropCallback(base::BindRepeating(
        [](NoteBackground* host) {
          std::unique_ptr<views::InkDrop> ink_drop =
              views::InkDrop::CreateInkDropWithoutAutoHighlight(
                  views::InkDrop::Get(host), /*highlight_on_hover=*/false);
          ink_drop->AddObserver(host->observer_);
          return ink_drop;
        },
        this));
    views::InkDrop::Get(this)->SetCreateRippleCallback(base::BindRepeating(
        [](NoteBackground* host) -> std::unique_ptr<views::InkDropRipple> {
          const gfx::Point center = base::i18n::IsRTL()
                                        ? host->GetLocalBounds().origin()
                                        : host->GetLocalBounds().top_right();
          auto ink_drop_ripple =
              std::make_unique<views::FloodFillInkDropRipple>(
                  views::InkDrop::Get(host), host->size(), gfx::Insets(),
                  center, views::InkDrop::Get(host)->GetBaseColor(), 1);
          ink_drop_ripple->set_use_hide_transform_duration_for_hide_fade_out(
              true);
          ink_drop_ripple->set_duration_factor(1.5);
          return ink_drop_ripple;
        },
        this));
    views::InkDrop::Get(this)->SetBaseColor(SK_ColorBLACK);
  }

  NoteBackground(const NoteBackground&) = delete;
  NoteBackground& operator=(const NoteBackground&) = delete;

  ~NoteBackground() override = default;

 private:
  raw_ptr<views::InkDropObserver> observer_;
};

BEGIN_METADATA(LockScreenActionBackgroundView, NoteBackground)
END_METADATA

LockScreenActionBackgroundView::LockScreenActionBackgroundView() {
  SetCanMaximize(true);
  SetCanFullscreen(true);

  auto layout_manager = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical);
  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);
  auto* layout_ptr = SetLayoutManager(std::move(layout_manager));

  background_ = new NoteBackground(this);
  AddChildView(background_.get());
  // Make background view flexible - the constant does not really matter given
  // that |background_| is the only child, as long as it's greater than 0.
  layout_ptr->SetFlexForView(background_, 1 /*flex_weight*/);
}

LockScreenActionBackgroundView::~LockScreenActionBackgroundView() = default;

void LockScreenActionBackgroundView::AnimateShow(base::OnceClosure done) {
  animation_end_callback_ = std::move(done);
  animating_to_state_ = views::InkDropState::ACTIVATED;

  views::InkDrop::Get(background_)
      ->AnimateToState(views::InkDropState::ACTIVATED, nullptr);
}

void LockScreenActionBackgroundView::AnimateHide(base::OnceClosure done) {
  animation_end_callback_ = std::move(done);
  animating_to_state_ = views::InkDropState::HIDDEN;

  views::InkDrop::Get(background_)
      ->AnimateToState(views::InkDropState::HIDDEN, nullptr);
}

void LockScreenActionBackgroundView::InkDropAnimationStarted() {}

void LockScreenActionBackgroundView::InkDropRippleAnimationEnded(
    views::InkDropState state) {
  // In case |AnimateShow| or |AnimateHide| is called before previous state
  // animation ends, this might get called with the previous target state
  // as the animation is aborted - ignore the event if the |state| does not
  // match the current target state.
  if (animation_end_callback_.is_null() || state != animating_to_state_)
    return;

  std::move(animation_end_callback_).Run();
}

bool LockScreenActionBackgroundView::CanActivate() const {
  return false;
}

views::View* LockScreenActionBackgroundView::GetBackgroundView() {
  return background_;
}

BEGIN_METADATA(LockScreenActionBackgroundView)
END_METADATA

}  // namespace ash
