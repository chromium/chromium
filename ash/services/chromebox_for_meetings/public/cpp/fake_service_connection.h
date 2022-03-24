// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_CHROMEBOX_FOR_MEETINGS_PUBLIC_CPP_FAKE_SERVICE_CONNECTION_H_
#define ASH_SERVICES_CHROMEBOX_FOR_MEETINGS_PUBLIC_CPP_FAKE_SERVICE_CONNECTION_H_

#include "ash/services/chromebox_for_meetings/public/cpp/service_connection.h"
#include "ash/services/chromebox_for_meetings/public/mojom/cfm_service_manager.mojom.h"
#include "base/bind.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chromeos {
namespace cfm {

// Fake implementation of chromeos::cfm::ServiceConnection.
// For use with ServiceConnection::UseFakeServiceConnectionForTesting().
class FakeServiceConnectionImpl : public ServiceConnection {
 public:
  using FakeBootstrapCallback =
      base::OnceCallback<void(mojo::PendingReceiver<mojom::CfmServiceContext>,
                              bool)>;

  FakeServiceConnectionImpl();
  FakeServiceConnectionImpl(const FakeServiceConnectionImpl&) = delete;
  FakeServiceConnectionImpl& operator=(const FakeServiceConnectionImpl&) =
      delete;
  ~FakeServiceConnectionImpl() override;

  void BindServiceContext(mojo::PendingReceiver<mojom::CfmServiceContext>
                              pending_receiver) override;

  void SetCallback(FakeBootstrapCallback callback);

 private:
  void CfMContextServiceStarted(
      mojo::PendingReceiver<mojom::CfmServiceContext> pending_receiver,
      bool is_available);

  FakeBootstrapCallback callback_;
};

}  // namespace cfm
}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the migration is finished.
namespace ash::cfm {
using ::chromeos::cfm::FakeServiceConnectionImpl;
}

#endif  // ASH_SERVICES_CHROMEBOX_FOR_MEETINGS_PUBLIC_CPP_FAKE_SERVICE_CONNECTION_H_
