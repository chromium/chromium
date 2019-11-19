// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ash_service.h"

#include "ash/mojo_interface_factory.h"

namespace ash {

AshService::AshService(service_manager::mojom::ServiceRequest request)
    : service_binding_(this, std::move(request)) {}

AshService::~AshService() = default;

void AshService::OnStart() {
  mojo_interface_factory::RegisterInterfaces(
      &registry_, base::ThreadTaskRunnerHandle::Get());
}

void AshService::OnBindInterface(
    const service_manager::BindSourceInfo& remote_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle handle) {
  registry_.BindInterface(interface_name, std::move(handle));
}

}  // namespace ash
