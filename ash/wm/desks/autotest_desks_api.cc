// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/autotest_desks_api.h"

#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_histogram_enums.h"
#include "base/callback.h"
#include "base/logging.h"

namespace ash {

namespace {

class DeskAnimationObserver : public DesksController::Observer {
 public:
  DeskAnimationObserver(base::OnceClosure on_desk_animation_complete)
      : on_desk_animation_complete_(std::move(on_desk_animation_complete)) {
    DesksController::Get()->AddObserver(this);
  }

  ~DeskAnimationObserver() override {
    DesksController::Get()->RemoveObserver(this);
  }

  DeskAnimationObserver(const DeskAnimationObserver& other) = delete;
  DeskAnimationObserver& operator=(const DeskAnimationObserver& rhs) = delete;

  // DesksController::Observer:
  void OnDeskAdded(const Desk* desk) override {}
  void OnDeskRemoved(const Desk* desk) override {}
  void OnDeskActivationChanged(const Desk* activated,
                               const Desk* deactivated) override {}
  void OnDeskSwitchAnimationLaunching() override {}
  void OnDeskSwitchAnimationFinished() override {
    std::move(on_desk_animation_complete_).Run();
    delete this;
  }

 private:
  base::OnceClosure on_desk_animation_complete_;
};

}  // namespace

AutotestDesksApi::AutotestDesksApi() = default;

AutotestDesksApi::~AutotestDesksApi() = default;

bool AutotestDesksApi::CreateNewDesk() {
  if (!DesksController::Get()->CanCreateDesks())
    return false;

  DesksController::Get()->NewDesk(DesksCreationRemovalSource::kButton);
  return true;
}

bool AutotestDesksApi::ActivateDeskAtIndex(int index,
                                           base::OnceClosure on_complete) {
  DCHECK(!on_complete.is_null());

  if (index < 0)
    return false;

  auto* controller = DesksController::Get();
  if (index >= int{controller->desks().size()})
    return false;

  const Desk* target_desk = controller->desks()[index].get();
  if (target_desk == controller->active_desk())
    return false;

  new DeskAnimationObserver(std::move(on_complete));
  controller->ActivateDesk(target_desk, DesksSwitchSource::kMiniViewButton);
  return true;
}

bool AutotestDesksApi::RemoveActiveDesk(base::OnceClosure on_complete) {
  DCHECK(!on_complete.is_null());

  auto* controller = DesksController::Get();
  if (!controller->CanRemoveDesks())
    return false;

  new DeskAnimationObserver(std::move(on_complete));
  controller->RemoveDesk(controller->active_desk(),
                         DesksCreationRemovalSource::kButton);
  return true;
}

}  // namespace ash
