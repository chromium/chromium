// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/system_shadow.h"
#include "base/logging.h"
#include "base/notreached.h"

namespace ash {

SystemShadow::SystemShadow(Type type) : type_(type) {
  // Note: Init function should be called before SetShadowStyle.
  Init(GetElevationFromType(type_));
  // System shadow always use `kChromeOSSystemUI` as shadow style.
  SetShadowStyle(gfx::ShadowStyle::kChromeOSSystemUI);
}

SystemShadow::~SystemShadow() = default;

int SystemShadow::GetElevationFromType(Type type) {
  switch (type) {
    case Type::kElevation4:
      return 4;
    case Type::kElevation8:
      return 8;
    case Type::kElevation12:
      return 12;
    case Type::kElevation16:
      return 16;
    case Type::kElevation24:
      return 24;
  }
}

void SystemShadow::SetType(Type type) {
  if (type_ == type)
    return;

  type_ = type;
  SetElevation(GetElevationFromType(type_));
}

}  // namespace ash
