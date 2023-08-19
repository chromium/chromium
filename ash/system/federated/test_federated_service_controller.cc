// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/federated/test_federated_service_controller.h"

namespace ash::federated {

bool TestFederatedServiceController::IsServiceAvailable() const {
  return true;
}

}  // namespace ash::federated
