// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/webui/diagnostics_ui/backend/common/routine_properties.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace diagnostics {

TEST(RoutineTypeUtilTtest, RoutinePropertiesListUpToDate) {
  EXPECT_EQ(kRoutinePropertiesLength,
            static_cast<size_t>(mojom::RoutineType::kMaxValue) + 1);
  for (size_t i = 0; i < kRoutinePropertiesLength; i++) {
    EXPECT_EQ(static_cast<mojom::RoutineType>(i), kRoutineProperties[i].type);
  }
}

}  // namespace diagnostics
}  // namespace ash
