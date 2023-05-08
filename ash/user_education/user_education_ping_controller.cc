// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/user_education_ping_controller.h"

#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_owner.h"
#include "ui/views/view.h"

namespace ash {
namespace {

// The singleton instance owned by the `UserEducationController`.
UserEducationPingController* g_instance = nullptr;

}  // namespace

// UserEducationPingController::Ping -------------------------------------------

class UserEducationPingController::Ping {
 public:
  explicit Ping(views::View* view)
      : view_(view),
        layer_owner_(std::make_unique<ui::Layer>(ui::LAYER_NOT_DRAWN)) {
    CHECK(view_->layer());

    // Name ping layers so that they are easy to identify in debugging/testing.
    layer_owner_.layer()->SetName(kPingLayerName);

    // Add ping layers to the layer tree below `view_` layers so that the ping
    // appears beneath the `view_`.
    view_->AddLayerToRegion(layer_owner_.layer(), views::LayerRegion::kBelow);
  }

  Ping(const Ping&) = delete;
  Ping& operator=(const Ping&) = delete;
  ~Ping() = default;

  // Returns the `view_` associated with this ping.
  const views::View* view() const { return view_; }

 private:
  // Pointer to the view associated with this ping.
  const raw_ptr<views::View> view_;

  // The owner for the ping layer which is added to the layer tree below `view_`
  // layers so that the ping appears beneath the `view_`.
  ui::LayerOwner layer_owner_;
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
