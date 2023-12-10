// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/session/arc_instance_mode.h"

#include <string>

#include "base/logging.h"

namespace arc {
namespace {

std::string ArcInstanceModeToString(ArcInstanceMode mode) {
#define MAP_MODE(name)        \
  case ArcInstanceMode::name: \
    return #name

  switch (mode) {
    MAP_MODE(MINI_INSTANCE);
    MAP_MODE(FULL_INSTANCE);
  }
#undef MAP_MODE

  // Some compilers report an error even if all values of an enum-class are
  // covered exhaustively in a switch statement.
  LOG(ERROR) << "Invalid value " << static_cast<int>(mode);
  return std::string();
}

}  // namespace

std::ostream& operator<<(std::ostream& os, ArcInstanceMode mode) {
  return os << ArcInstanceModeToString(mode);
}

std::ostream& operator<<(std::ostream& os,
                         std::optional<ArcInstanceMode> mode) {
  return os << (mode.has_value() ? ArcInstanceModeToString(mode.value())
                                 : "(nullopt)");
}

}  // namespace arc
