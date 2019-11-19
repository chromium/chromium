// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SERVICE_REGISTRY_H_
#define CHROME_BROWSER_ASH_SERVICE_REGISTRY_H_

#include <string>

#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/mojom/service.mojom.h"

namespace ash_service_registry {

// Handles an incoming ServiceRequest to run a service in the browser process.
std::unique_ptr<service_manager::Service> HandleServiceRequest(
    const std::string& service_name,
    service_manager::mojom::ServiceRequest request);

}  // namespace ash_service_registry

#endif  // CHROME_BROWSER_ASH_SERVICE_REGISTRY_H_
