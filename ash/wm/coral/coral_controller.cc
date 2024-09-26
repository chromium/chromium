// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/coral/coral_controller.h"

#include "ash/public/cpp/coral_delegate.h"
#include "ash/shell.h"
#include "ash/wm/desks/desks_controller.h"

namespace ash {

CoralRequest::CoralRequest() = default;

CoralRequest::~CoralRequest() = default;

CoralResponse::CoralResponse() = default;

CoralResponse::~CoralResponse() = default;

CoralController::CoralController() = default;

CoralController::~CoralController() = default;

void CoralController::GenerateContentGroups(const CoralRequest& request,
                                            CoralResponseCallback callback) {
  // Not implemented yet.
  std::move(callback).Run(nullptr);
}

void CoralController::CacheEmbeddings(const CoralRequest& request,
                                      base::OnceCallback<void(bool)> callback) {
  // Not implemented yet.
  std::move(callback).Run(false);
}

void CoralController::OpenNewDeskWithGroup(CoralResponse::Group group) {
  if (group->entities.empty()) {
    return;
  }

  DesksController* desks_controller = DesksController::Get();
  if (!desks_controller->CanCreateDesks()) {
    return;
  }
  desks_controller->NewDesk(DesksCreationRemovalSource::kCoral,
                            base::UTF8ToUTF16(group->title));
  Shell::Get()->coral_delegate()->MoveTabsInGroupToNewDesk(std::move(group));

  // TODO(zxdan): move the apps in group to the new desk.
  desks_controller->ActivateDesk(desks_controller->desks().back().get(),
                                 DesksSwitchSource::kCoral);
}

}  // namespace ash
