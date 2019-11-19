// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/wilco_dtc_supportd/wilco_dtc_supportd_client.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/wilco_dtc_supportd/fake_wilco_dtc_supportd_client.h"
#include "chrome/common/chrome_features.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

WilcoDtcSupportdClient* g_instance = nullptr;

void OnVoidDBusMethod(VoidDBusMethodCallback callback,
                      dbus::Response* response) {
  std::move(callback).Run(response != nullptr);
}

// The WilcoDtcSupportdClient implementation used in production.
class WilcoDtcSupportdClientImpl final : public WilcoDtcSupportdClient {
 public:
  WilcoDtcSupportdClientImpl();
  ~WilcoDtcSupportdClientImpl() override;

  // WilcoDtcSupportdClient overrides:
  void WaitForServiceToBeAvailable(
      WaitForServiceToBeAvailableCallback callback) override;
  void BootstrapMojoConnection(base::ScopedFD fd,
                               VoidDBusMethodCallback callback) override;
  void Init(dbus::Bus* bus) override;

 private:
  dbus::ObjectProxy* wilco_dtc_supportd_proxy_ = nullptr;

  base::WeakPtrFactory<WilcoDtcSupportdClientImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WilcoDtcSupportdClientImpl);
};

WilcoDtcSupportdClientImpl::WilcoDtcSupportdClientImpl() = default;

WilcoDtcSupportdClientImpl::~WilcoDtcSupportdClientImpl() = default;

void WilcoDtcSupportdClientImpl::WaitForServiceToBeAvailable(
    WaitForServiceToBeAvailableCallback callback) {
  wilco_dtc_supportd_proxy_->WaitForServiceToBeAvailable(std::move(callback));
}

void WilcoDtcSupportdClientImpl::BootstrapMojoConnection(
    base::ScopedFD fd,
    VoidDBusMethodCallback callback) {
  dbus::MethodCall method_call(
      ::diagnostics::kWilcoDtcSupportdServiceInterface,
      ::diagnostics::kWilcoDtcSupportdBootstrapMojoConnectionMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendFileDescriptor(fd.get());
  wilco_dtc_supportd_proxy_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&OnVoidDBusMethod, std::move(callback)));
}

void WilcoDtcSupportdClientImpl::Init(dbus::Bus* bus) {
  wilco_dtc_supportd_proxy_ = bus->GetObjectProxy(
      ::diagnostics::kWilcoDtcSupportdServiceName,
      dbus::ObjectPath(::diagnostics::kWilcoDtcSupportdServicePath));
}

}  // namespace

WilcoDtcSupportdClient::WilcoDtcSupportdClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

WilcoDtcSupportdClient::~WilcoDtcSupportdClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void WilcoDtcSupportdClient::Initialize(dbus::Bus* bus) {
  DCHECK(bus);
#if defined(OS_CHROMEOS)
  if (base::FeatureList::IsEnabled(::features::kWilcoDtc)) {
    (new WilcoDtcSupportdClientImpl())->Init(bus);
  }
#endif
}

// static
void WilcoDtcSupportdClient::InitializeFake() {
  new FakeWilcoDtcSupportdClient();
}

// static
void WilcoDtcSupportdClient::Shutdown() {
  if (g_instance)
    delete g_instance;
}

// static
bool WilcoDtcSupportdClient::IsInitialized() {
  return g_instance;
}

// static
WilcoDtcSupportdClient* WilcoDtcSupportdClient::Get() {
  CHECK(IsInitialized());
  return g_instance;
}

}  // namespace chromeos
