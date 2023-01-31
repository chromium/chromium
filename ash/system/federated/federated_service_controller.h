// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FEDERATED_FEDERATED_SERVICE_CONTROLLER_H_
#define ASH_SYSTEM_FEDERATED_FEDERATED_SERVICE_CONTROLLER_H_

#include "ash/ash_export.h"

namespace ash::federated {

// Controller class for the federated service.
class ASH_EXPORT FederatedServiceController {
 public:
  virtual ~FederatedServiceController() = default;

  // Whether the federated service is available.
  virtual bool IsServiceAvailable() const = 0;
};

}  // namespace ash::federated

#endif  // ASH_SYSTEM_FEDERATED_FEDERATED_SERVICE_CONTROLLER_H_
