// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/session/arc_management_transition.h"
#include "base/notreached.h"

namespace arc {

std::ostream& operator<<(std::ostream& os,
                         ArcManagementTransition management_transition) {
  switch (management_transition) {
    case ArcManagementTransition::NO_TRANSITION:
      return os << "NO_TRANSITION";
    case ArcManagementTransition::CHILD_TO_REGULAR:
      return os << "CHILD_TO_REGULAR";
    case ArcManagementTransition::REGULAR_TO_CHILD:
      return os << "REGULAR_TO_CHILD";
    case ArcManagementTransition::UNMANAGED_TO_MANAGED:
      return os << "UNMANAGED_TO_MANAGED";
  }
  NOTREACHED() << "Unexpected value for ArcManagementTransition: "
               << static_cast<int>(management_transition);
}

}  // namespace arc
