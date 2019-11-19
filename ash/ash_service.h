// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASH_SERVICE_H_
#define ASH_ASH_SERVICE_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/mojom/service.mojom.h"

namespace ash {

// Used to export Ash's mojo services, specifically the interfaces defined in
// Ash's manifest.
class ASH_EXPORT AshService : public service_manager::Service {
 public:
  explicit AshService(service_manager::mojom::ServiceRequest request);
  ~AshService() override;

  // service_manager::Service:
  void OnStart() override;
  void OnBindInterface(const service_manager::BindSourceInfo& remote_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle handle) override;

 private:
  service_manager::ServiceBinding service_binding_;
  service_manager::BinderRegistry registry_;

  DISALLOW_COPY_AND_ASSIGN(AshService);
};

}  // namespace ash

#endif  // ASH_ASH_SERVICE_H_
