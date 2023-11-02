// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_SESSION_ARC_MANAGEMENT_TRANSITION_H_
#define ASH_COMPONENTS_ARC_SESSION_ARC_MANAGEMENT_TRANSITION_H_

#include <ostream>

namespace arc {

// These values must be kept in sync with
// UpgradeArcContainerRequest.ManagementTransition in
// third_party/cros_system_api/dbus/arc.proto.
enum class ArcManagementTransition : int {
  // No transition necessary.
  NO_TRANSITION = 0,
  // Child user is transitioning to a regular account, need to lift
  // supervision.
  CHILD_TO_REGULAR = 1,
  // Regular user is transitioning to a child account, need to enable
  // supervision.
  REGULAR_TO_CHILD = 2,
  // Unmanaged user is transitioning to a managed state, need to enable
  // management.
  UNMANAGED_TO_MANAGED = 3,
};

std::ostream& operator<<(std::ostream& os,
                         ArcManagementTransition managementTransition);

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_SESSION_ARC_MANAGEMENT_TRANSITION_H_
