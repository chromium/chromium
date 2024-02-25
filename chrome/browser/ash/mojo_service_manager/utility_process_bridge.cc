// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/mojo_service_manager/utility_process_bridge.h"

#include "base/no_destructor.h"
#include "chromeos/ash/components/mojo_service_manager/connection.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace ash::mojo_service_manager {

namespace mojom = chromeos::mojo_service_manager::mojom;

namespace {

class UtilityProcessBridge
    : chromeos::mojo_service_manager::mojom::ServiceManager {
 public:
  UtilityProcessBridge(const UtilityProcessBridge&) = delete;
  UtilityProcessBridge& operator=(const UtilityProcessBridge&) = delete;

  void Register(const std::string& service_name,
                mojo::PendingRemote<
                    chromeos::mojo_service_manager::mojom::ServiceProvider>
                    service_provider) override {
    GetServiceManagerProxy()->Register(service_name,
                                       std::move(service_provider));
  }

  void Request(const std::string& service_name,
               std::optional<base::TimeDelta> timeout,
               mojo::ScopedMessagePipeHandle receiver) override {
    GetServiceManagerProxy()->Request(service_name, std::move(timeout),
                                      std::move(receiver));
  }

  using QueryCallback =
      chromeos::mojo_service_manager::mojom::ServiceManager::QueryCallback;

  void Query(const std::string& service_name, QueryCallback callback) override {
    GetServiceManagerProxy()->Query(service_name, std::move(callback));
  }

  void AddServiceObserver(
      mojo::PendingRemote<
          chromeos::mojo_service_manager::mojom::ServiceObserver> observer)
      override {
    GetServiceManagerProxy()->AddServiceObserver(std::move(observer));
  }

  void BindReceiver(mojo::PendingReceiver<
                    chromeos::mojo_service_manager::mojom::ServiceManager>
                        pending_receiver) {
    receivers_.Add(this, std::move(pending_receiver));
  }

  static UtilityProcessBridge* GetInstance() {
    static base::NoDestructor<UtilityProcessBridge> instance;
    return instance.get();
  }

 private:
  friend base::NoDestructor<UtilityProcessBridge>;

  UtilityProcessBridge() = default;
  ~UtilityProcessBridge() override = default;

  mojo::ReceiverSet<chromeos::mojo_service_manager::mojom::ServiceManager>
      receivers_;
};

}  // namespace

void EstablishUtilityProcessBridge(
    mojo::PendingReceiver<mojom::ServiceManager> pending_receiver) {
  UtilityProcessBridge::GetInstance()->BindReceiver(
      std::move(pending_receiver));
}

}  // namespace ash::mojo_service_manager
