// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_SECURE_CHANNEL_CLIENT_CONNECTION_PARAMETERS_IMPL_H_
#define ASH_SERVICES_SECURE_CHANNEL_CLIENT_CONNECTION_PARAMETERS_IMPL_H_

#include "ash/services/secure_channel/client_connection_parameters.h"
#include "ash/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::secure_channel {

// Concrete ClientConnectionParameters implementation, which utilizes a
// mojo::Remote<ConnectionDelegate>.
class ClientConnectionParametersImpl : public ClientConnectionParameters {
 public:
  class Factory {
   public:
    static std::unique_ptr<ClientConnectionParameters> Create(
        const std::string& feature,
        mojo::PendingRemote<chromeos::secure_channel::mojom::ConnectionDelegate>
            connection_delegate_remote);
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<ClientConnectionParameters> CreateInstance(
        const std::string& feature,
        mojo::PendingRemote<chromeos::secure_channel::mojom::ConnectionDelegate>
            connection_delegate_remote) = 0;

   private:
    static Factory* test_factory_;
  };

  ClientConnectionParametersImpl(const ClientConnectionParametersImpl&) =
      delete;
  ClientConnectionParametersImpl& operator=(
      const ClientConnectionParametersImpl&) = delete;

  ~ClientConnectionParametersImpl() override;

 private:
  ClientConnectionParametersImpl(
      const std::string& feature,
      mojo::PendingRemote<chromeos::secure_channel::mojom::ConnectionDelegate>
          connection_delegate_remote);

  // ClientConnectionParameters:
  bool HasClientCanceledRequest() override;
  void PerformSetConnectionAttemptFailed(
      chromeos::secure_channel::mojom::ConnectionAttemptFailureReason reason)
      override;
  void PerformSetConnectionSucceeded(
      mojo::PendingRemote<chromeos::secure_channel::mojom::Channel> channel,
      mojo::PendingReceiver<chromeos::secure_channel::mojom::MessageReceiver>
          message_receiver_receiver) override;

  void OnConnectionDelegateRemoteDisconnected();

  mojo::Remote<chromeos::secure_channel::mojom::ConnectionDelegate>
      connection_delegate_remote_;
};

}  // namespace ash::secure_channel

// TODO(https://crbug.com/1164001): remove after the migration is finished.
namespace chromeos::secure_channel {
using ::ash::secure_channel::ClientConnectionParametersImpl;
}

#endif  // ASH_SERVICES_SECURE_CHANNEL_CLIENT_CONNECTION_PARAMETERS_IMPL_H_
