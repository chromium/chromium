// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/user_education_ping_controller.h"

#include "ash/public/cpp/style/dark_light_mode_controller.h"
#include "ash/user_education/user_education_class_properties.h"
#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_owner.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace ash {
namespace {

// The singleton instance owned by the `UserEducationController`.
UserEducationPingController* g_instance = nullptr;

// Helpers ---------------------------------------------------------------------

// Comparable to `gfx::Rect::ClampToCenteredSize()` except max size is used
// instead of min size when determining the new rect.
gfx::Rect EnlargeToCenteredSize(const gfx::Rect& rect, const gfx::Size& size) {
  const int width = std::max(rect.width(), size.width());
  const int height = std::max(rect.height(), size.height());
  const int x = rect.x() + (rect.width() - width) / 2;
  const int y = rect.y() + (rect.height() - height) / 2;
  return gfx::Rect(x, y, width, height);
}

gfx::Size EnlargeToSquare(const gfx::Size& size) {
  const int max = std::max(size.width(), size.height());
  return gfx::Size(max, max);
}

gfx::Rect Inset(const gfx::Rect& rect, const gfx::Insets* insets) {
  gfx::Rect inset_rect(rect);
  if (insets) {
    inset_rect.Inset(*insets);
  }
  return inset_rect;
}

}  // namespace

// UserEducationPingController::Ping -------------------------------------------

class UserEducationPingController::Ping : public views::ViewObserver {
 public:
  explicit Ping(views::View* view)
      : view_(view),
        parent_(std::make_unique<ui::Layer>(ui::LAYER_NOT_DRAWN)),
        child_(std::make_unique<ui::Layer>(ui::LAYER_SOLID_COLOR)) {
    CHECK(view_->layer());

    // Name ping layers so that they are easy to identify in debugging/testing.
    parent_.layer()->SetName(kPingParentLayerName);
    child_.layer()->SetName(kPingChildLayerName);

    // Configure `child_` layer properties.
    child_.layer()->SetFillsBoundsOpaquely(false);
    OnViewThemeChanged(view_);

    // Add ping layers to the layer tree below `view_` layers. This is done so
    // that the ping appears to be beneath the associated `view_` to the user.
    parent_.layer()->Add(child_.layer());
    view_->AddLayerToRegion(parent_.layer(), views::LayerRegion::kBelow);

    // Observe `view_` to keep ping layers in sync.
    view_observation_.Observe(view_);

    // Initialize.
    Update();
  }

  Ping(const Ping&) = delete;
  Ping& operator=(const Ping&) = delete;
  ~Ping() override = default;

  // Returns the `view_` associated with this ping.
  const views::View* view() const { return view_; }

 private:
  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* view) override {
    // Update ping to stay in sync with `view_` bounds.
    Update();
  }

  void OnViewPropertyChanged(views::View* view,
                             const void* key,
                             int64_t old_value) override {
    // Update ping to stay in sync with requested insets.
    if (key == kPingInsetsKey) {
      Update();
    }
  }

  // TODO(http://b/281536915): Replace with semantic color.
  void OnViewThemeChanged(views::View* view) override {
    child_.layer()->SetColor(DarkLightModeController::Get()->IsDarkModeEnabled()
                                 ? SK_ColorWHITE
                                 : SK_ColorBLACK);
  }

  // TODO(http://b/281536915): Implement animation.
  // Updates the ping for the current state.
  void Update() {
    ui::Layer* const parent = parent_.layer();
    ui::Layer* const child = child_.layer();

    // Match `parent` bounds to that of the associated `view_` layer. Because
    // `parent` was added as a top-level layer beneath the `view_` layer, they
    // are siblings and will share the same origin even if not explicitly set.
    parent->SetBounds(view_->layer()->bounds());

    // Set `child` bounds based on the size and center point of its `parent`.
    // Because `child` was added to the `parent` layer, it is not a top-level
    // layer beneath the `view_` layer and will therefore not be forced to share
    // the same origin. Note that `child` bounds respect ping insets and are
    // always square.
    gfx::Rect bounds(parent->size());
    bounds = Inset(bounds, view_->GetProperty(kPingInsetsKey));
    bounds = EnlargeToCenteredSize(bounds, EnlargeToSquare(bounds.size()));
    child->SetBounds(bounds);

    // Clip `child` to a circle.
    CHECK_EQ(bounds.width(), bounds.height());
    child->SetRoundedCornerRadius(gfx::RoundedCornersF(bounds.width() / 2.f));
  }

  // Pointer to the view associated with this ping.
  const raw_ptr<views::View> view_;

  // Owners for the ping layers which are added to the layer tree below `view_`
  // layers. This is done so that the ping appears to be beneath the associated
  // `view_` to the user. Note that top-level layers added below view layers
  // always share the same origin as the view layer, so a `child_` layer is
  // needed in order to achieve desired bounds for the ping.
  ui::LayerOwner parent_;
  ui::LayerOwner child_;

  // Observe `view_` in order to keep the associated ping in sync.
  base::ScopedObservation<views::View, views::ViewObserver> view_observation_{
      this};
};

// UserEducationPingController -------------------------------------------------

UserEducationPingController::UserEducationPingController() {
  CHECK_EQ(g_instance, nullptr);
  g_instance = this;
}

UserEducationPingController::~UserEducationPingController() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
UserEducationPingController* UserEducationPingController::Get() {
  return g_instance;
}

bool UserEducationPingController::CreatePing(PingId ping_id,
                                             views::View* view) {
  // A ping is not created if a ping already exists for `ping_id` or `view`.
  for (const auto& [id, ping] : pings_by_id_) {
    if (id == ping_id || ping->view() == view) {
      return false;
    }
  }

  // A ping isn't created if `view` is not drawn.
  if (!view->IsDrawn()) {
    return false;
  }

  // Create a ping and indicate success.
  pings_by_id_[ping_id] = std::make_unique<Ping>(view);
  return true;
}

}  // namespace ash
