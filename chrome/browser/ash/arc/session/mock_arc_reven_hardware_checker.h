// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_SESSION_MOCK_ARC_REVEN_HARDWARE_CHECKER_H_
#define CHROME_BROWSER_ASH_ARC_SESSION_MOCK_ARC_REVEN_HARDWARE_CHECKER_H_

#include <gmock/gmock.h>

#include "chrome/browser/ash/arc/session/arc_reven_hardware_checker.h"

namespace arc {
class MockArcRevenHardwareChecker : public ArcRevenHardwareChecker {
 public:
  MockArcRevenHardwareChecker();
  MockArcRevenHardwareChecker(const MockArcRevenHardwareChecker&) = delete;
  MockArcRevenHardwareChecker& operator=(const MockArcRevenHardwareChecker&) =
      delete;
  ~MockArcRevenHardwareChecker() override;

  MOCK_METHOD(void,
              IsRevenDeviceCompatibleForArc,
              (base::OnceCallback<void(bool)> callback),
              (override));
};

MockArcRevenHardwareChecker::MockArcRevenHardwareChecker() = default;

MockArcRevenHardwareChecker::~MockArcRevenHardwareChecker() = default;

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_SESSION_MOCK_ARC_REVEN_HARDWARE_CHECKER_H_
