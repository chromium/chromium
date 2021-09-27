// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/identity_manager_lacros.h"

#include "chromeos/lacros/lacros_service.h"

namespace {

using RemoteMinVersions = crosapi::mojom::IdentityManager::MethodMinVersions;

chromeos::LacrosService* GetLacrosService(int min_version,
                                          const std::string& function_name) {
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  if (!service)
    return nullptr;
  int interface_version =
      service->GetInterfaceVersion(crosapi::mojom::IdentityManager::Uuid_);
  if (interface_version < min_version) {
    DLOG(ERROR) << "Unsupported ash version for " << function_name;
    return nullptr;
  }

  return service;
}

}  // namespace

IdentityManagerLacros::IdentityManagerLacros() {}

IdentityManagerLacros::~IdentityManagerLacros() = default;

void IdentityManagerLacros::GetAccountFullName(
    const std::string& gaia_id,
    crosapi::mojom::IdentityManager::GetAccountFullNameCallback callback) {
  chromeos::LacrosService* service = GetLacrosService(
      RemoteMinVersions::kGetAccountFullNameMinVersion, "GetAccountFullName");
  if (!service) {
    std::move(callback).Run("");
    return;
  }

  service->GetRemote<crosapi::mojom::IdentityManager>()->GetAccountFullName(
      gaia_id, std::move(callback));
}

void IdentityManagerLacros::GetAccountImage(
    const std::string& gaia_id,
    crosapi::mojom::IdentityManager::GetAccountImageCallback callback) {
  chromeos::LacrosService* service = GetLacrosService(
      RemoteMinVersions::kGetAccountImageMinVersion, "GetAccountImage");
  if (!service) {
    std::move(callback).Run(gfx::ImageSkia());
    return;
  }

  service->GetRemote<crosapi::mojom::IdentityManager>()->GetAccountImage(
      gaia_id, std::move(callback));
}
