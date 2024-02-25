// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_SESSION_ARC_INSTANCE_MODE_H_
#define ASH_COMPONENTS_ARC_SESSION_ARC_INSTANCE_MODE_H_

#include <optional>
#include <ostream>

namespace arc {

enum class ArcInstanceMode {
  // Instance running starting on login screen. We're planning to expand its
  // usage. cf) b/62701318.
  MINI_INSTANCE,

  // Fully set up instance. Specifically, it should support Mojo connection.
  FULL_INSTANCE,
};

// Stringified output for logging purpose.
std::ostream& operator<<(std::ostream& os, ArcInstanceMode mode);
std::ostream& operator<<(std::ostream& os, std::optional<ArcInstanceMode> mode);

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_SESSION_ARC_INSTANCE_MODE_H_
