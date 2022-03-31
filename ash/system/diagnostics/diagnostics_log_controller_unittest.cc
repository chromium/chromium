// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/diagnostics/diagnostics_log_controller.h"

#include <memory>

#include "ash/test/ash_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace diagnostics {

class DiagnosticsLogControllerTest : public NoSessionAshTestBase {
 public:
  DiagnosticsLogControllerTest() = default;
  DiagnosticsLogControllerTest(DiagnosticsLogControllerTest&) = delete;
  DiagnosticsLogControllerTest& operator=(DiagnosticsLogControllerTest&) =
      delete;
  ~DiagnosticsLogControllerTest() override = default;
};

}  // namespace diagnostics
}  // namespace ash
