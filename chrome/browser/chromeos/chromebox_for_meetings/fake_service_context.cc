// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/chromebox_for_meetings/fake_service_context.h"

namespace chromeos {
namespace cfm {

FakeCfmServiceContext::FakeCfmServiceContext() = default;
FakeCfmServiceContext::~FakeCfmServiceContext() = default;

void FakeCfmServiceContext::ProvideAdaptor(
    const std::string& interface_name,
    mojo::PendingRemote<mojom::CfmServiceAdaptor> adaptor_remote,
    ProvideAdaptorCallback callback) {
  std::move(provide_adaptor_callback_)
      .Run(std::move(interface_name), std::move(adaptor_remote),
           std::move(callback));
}

void FakeCfmServiceContext::BindRegistry(
    const std::string& interface_name,
    mojo::PendingReceiver<mojom::CfmServiceRegistry> broker_receiver,
    BindRegistryCallback callback) {
  std::move(bind_registry_callback_)
      .Run(std::move(interface_name), std::move(broker_receiver),
           std::move(callback));
}

void FakeCfmServiceContext::SetFakeProvideAdaptorCallback(
    FakeProvideAdaptorCallback callback) {
  provide_adaptor_callback_ = std::move(callback);
}

void FakeCfmServiceContext::SetFakeBindRegistryCallback(
    FakeBindRegistryCallback callback) {
  bind_registry_callback_ = std::move(callback);
}

}  // namespace cfm
}  // namespace chromeos
