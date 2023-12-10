// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/login_screen_storage_ash.h"

#include "chrome/common/extensions/api/login_screen_storage.h"
#include "chromeos/ash/components/dbus/login_manager/login_screen_storage.pb.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/crosapi/mojom/login_screen_storage.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace crosapi {

LoginScreenStorageAsh::LoginScreenStorageAsh() {}
LoginScreenStorageAsh::~LoginScreenStorageAsh() = default;

void LoginScreenStorageAsh::BindReceiver(
    mojo::PendingReceiver<mojom::LoginScreenStorage> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void LoginScreenStorageAsh::Store(
    const std::vector<std::string>& keys,
    crosapi::mojom::LoginScreenStorageMetadataPtr metadata,
    const std::string& data,
    StoreCallback callback) {
  login_manager::LoginScreenStorageMetadata metadata_dbus;
  metadata_dbus.set_clear_on_session_exit(metadata->clear_on_session_exit);

  StoreInternal(std::move(keys), metadata_dbus, data, std::move(callback));
}

void LoginScreenStorageAsh::StoreInternal(
    std::vector<std::string> keys,
    const login_manager::LoginScreenStorageMetadata& metadata,
    const std::string& data,
    StoreCallback callback) {
  if (keys.empty()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  const std::string key = keys.back();
  keys.pop_back();

  auto dbus_callback = base::BindOnce(
      &LoginScreenStorageAsh::OnStored, weak_ptr_factory_.GetWeakPtr(),
      std::move(keys), metadata, data, std::move(callback));
  ash::SessionManagerClient::Get()->LoginScreenStorageStore(
      key, metadata, data, std::move(dbus_callback));
}

void LoginScreenStorageAsh::OnStored(
    std::vector<std::string> remaining_keys,
    const login_manager::LoginScreenStorageMetadata& metadata,
    const std::string& data,
    StoreCallback callback,
    std::optional<std::string> error) {
  if (error) {
    std::move(callback).Run(error);
    return;
  }

  if (remaining_keys.empty()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  StoreInternal(std::move(remaining_keys), metadata, data, std::move(callback));
}

void LoginScreenStorageAsh::Retrieve(const std::string& key,
                                     RetrieveCallback callback) {
  auto dbus_callback =
      base::BindOnce(&LoginScreenStorageAsh::OnRetrieved,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback));
  ash::SessionManagerClient::Get()->LoginScreenStorageRetrieve(
      key, std::move(dbus_callback));
}

void LoginScreenStorageAsh::OnRetrieved(RetrieveCallback callback,
                                        std::optional<std::string> data,
                                        std::optional<std::string> error) {
  mojom::LoginScreenStorageRetrieveResultPtr result;
  if (error) {
    result = mojom::LoginScreenStorageRetrieveResult::NewErrorMessage(*error);
  } else if (data) {
    result = mojom::LoginScreenStorageRetrieveResult::NewData(*data);
  }
  std::move(callback).Run(std::move(result));
}

}  // namespace crosapi
