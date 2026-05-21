// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/test_support/mock_glic_instance_coordinator.h"

namespace glic {

MockGlicInstanceCoordinator::MockGlicInstanceCoordinator() {
  ON_CALL(*this, active_instance_sharing_manager())
      .WillByDefault(testing::ReturnRef(dummy_sharing_manager_));
}
MockGlicInstanceCoordinator::~MockGlicInstanceCoordinator() = default;

}  // namespace glic
