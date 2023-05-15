// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/identity_manager_lacros.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chromeos/lacros/lacros_service.h"

namespace {

using RemoteMinVersions = crosapi::mojom::IdentityManager::MethodMinVersions;

chromeos::LacrosService* GetLacrosService(int min_version,
                                          const std::string& function_name) {
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  if (!service)
    return nullptr;
  int interface_version =
      service->GetInterfaceVersion<crosapi::mojom::IdentityManager>();
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
      gaia_id,
      base::BindOnce(&IdentityManagerLacros::RunFullNameCallback,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
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
      gaia_id,
      base::BindOnce(&IdentityManagerLacros::RunImageCallback,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void IdentityManagerLacros::GetAccountEmail(
    const std::string& gaia_id,
    crosapi::mojom::IdentityManager::GetAccountEmailCallback callback) {
  chromeos::LacrosService* service = GetLacrosService(
      RemoteMinVersions::kGetAccountEmailMinVersion, "GetAccountEmail");
  if (!service) {
    std::move(callback).Run("");
    return;
  }

  service->GetRemote<crosapi::mojom::IdentityManager>()->GetAccountEmail(
      gaia_id,
      base::BindOnce(&IdentityManagerLacros::RunEmailCallback,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void IdentityManagerLacros::HasAccountWithPersistentError(
    const std::string& gaia_id,
    crosapi::mojom::IdentityManager::HasAccountWithPersistentErrorCallback
        callback) {
  chromeos::LacrosService* service = GetLacrosService(
      RemoteMinVersions::kHasAccountWithPersistentErrorMinVersion,
      "HasAccountWithPersistentError");
  if (!service) {
    std::move(callback).Run(false);
    return;
  }

  service->GetRemote<crosapi::mojom::IdentityManager>()
      ->HasAccountWithPersistentError(
          gaia_id,
          base::BindOnce(&IdentityManagerLacros::RunPersistentErrorCallback,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void IdentityManagerLacros::RunFullNameCallback(
    crosapi::mojom::IdentityManager::GetAccountFullNameCallback callback,
    const std::string& name) {
  std::move(callback).Run(name);
}

void IdentityManagerLacros::RunImageCallback(
    crosapi::mojom::IdentityManager::GetAccountImageCallback callback,
    const gfx::ImageSkia& image) {
  std::move(callback).Run(image);
}

void IdentityManagerLacros::RunEmailCallback(
    crosapi::mojom::IdentityManager::GetAccountEmailCallback callback,
    const std::string& email) {
  std::move(callback).Run(email);
}

void IdentityManagerLacros::RunPersistentErrorCallback(
    crosapi::mojom::IdentityManager::HasAccountWithPersistentErrorCallback
        callback,
    bool persistent_error) {
  std::move(callback).Run(persistent_error);
}
