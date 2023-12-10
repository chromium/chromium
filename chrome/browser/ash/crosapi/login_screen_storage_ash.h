// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_LOGIN_SCREEN_STORAGE_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_LOGIN_SCREEN_STORAGE_ASH_H_

#include "chromeos/ash/components/dbus/login_manager/login_screen_storage.pb.h"
#include "chromeos/crosapi/mojom/login_screen_storage.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace crosapi {

// The ash-chrome implementation of the LoginScreenStorage crosapi interface.
class LoginScreenStorageAsh : public mojom::LoginScreenStorage {
 public:
  LoginScreenStorageAsh();
  LoginScreenStorageAsh(const LoginScreenStorageAsh&) = delete;
  LoginScreenStorageAsh& operator=(const LoginScreenStorageAsh&) = delete;
  ~LoginScreenStorageAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::LoginScreenStorage> receiver);

  // crosapi::mojom::LoginScreenStorage:
  // Stores |data| to the login screen storage. It takes a list of keys since
  // the data needs to be accessible from multiple extensions. |metadata|
  // specifies whether the data should be cleared on session exit.
  void Store(const std::vector<std::string>& keys,
             crosapi::mojom::LoginScreenStorageMetadataPtr metadata,
             const std::string& data,
             StoreCallback callback) override;

  // Retrieves the data that was previously stored for |key|.
  void Retrieve(const std::string& key, RetrieveCallback callback) override;

 private:
  // Stores |data| for every key in |keys|.
  void StoreInternal(std::vector<std::string> keys,
                     const login_manager::LoginScreenStorageMetadata& metadata,
                     const std::string& data,
                     StoreCallback callback);

  // Passed as a callback to the `LoginScreenStorageStore` D-Bus method. It is
  // called when |data| was stored for one of the keys. |remaining_keys| is the
  // list of keys that the data hasn't been stored for yet.
  void OnStored(std::vector<std::string> remaining_keys,
                const login_manager::LoginScreenStorageMetadata& metadata,
                const std::string& data,
                StoreCallback callback,
                std::optional<std::string> error);

  // Passed as a callback to the `LoginScreenStorageRetrieve` D-Bus method.
  void OnRetrieved(RetrieveCallback callback,
                   std::optional<std::string> data,
                   std::optional<std::string> error);

  // This class supports any number of connections.
  mojo::ReceiverSet<mojom::LoginScreenStorage> receivers_;

  base::WeakPtrFactory<LoginScreenStorageAsh> weak_ptr_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_LOGIN_SCREEN_STORAGE_ASH_H_
