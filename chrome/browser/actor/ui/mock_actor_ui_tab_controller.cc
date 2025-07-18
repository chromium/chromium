// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/mock_actor_ui_tab_controller.h"

namespace actor::ui {

MockActorUiTabController::MockActorUiTabController() {
  ON_CALL(*this, GetWeakPtr())
      .WillByDefault(testing::Return(weak_factory_.GetWeakPtr()));
}

MockActorUiTabController::~MockActorUiTabController() = default;

}  // namespace actor::ui
