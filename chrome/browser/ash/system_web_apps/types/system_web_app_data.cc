// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/types/system_web_app_data.h"
#include <ios>
#include <ostream>
#include <tuple>

namespace ash {

base::Value SystemWebAppData::AsDebugValue() const {
  return base::Value(base::Value::Dict().Set(
      "system_app_type", static_cast<int>(system_app_type)));
}

bool operator==(const SystemWebAppData& chromeos_data1,
                const SystemWebAppData& chromeos_data2) {
  return chromeos_data1.system_app_type == chromeos_data2.system_app_type;
}

}  // namespace ash
