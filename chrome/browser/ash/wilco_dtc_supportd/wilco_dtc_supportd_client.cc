// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/wilco_dtc_supportd/wilco_dtc_supportd_client.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/wilco_dtc_supportd/fake_wilco_dtc_supportd_client.h"
#include "chrome/common/chrome_features.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

WilcoDtcSupportdClient* g_instance = nullptr;

void OnVoidDBusMethod(chromeos::VoidDBusMethodCallback callback,
                      dbus::Response* response) {
  std::move(callback).Run(response != nullptr);
}

// The WilcoDtcSupportdClient implementation used in production.
class WilcoDtcSupportdClientImpl final : public WilcoDtcSupportdClient {
 public:
  WilcoDtcSupportdClientImpl();

  WilcoDtcSupportdClientImpl(const WilcoDtcSupportdClientImpl&) = delete;
  WilcoDtcSupportdClientImpl& operator=(const WilcoDtcSupportdClientImpl&) =
      delete;

  ~WilcoDtcSupportdClientImpl() override;

  // WilcoDtcSupportdClient overrides:
  void WaitForServiceToBeAvailable(
      chromeos::WaitForServiceToBeAvailableCallback callback) override;
  void BootstrapMojoConnection(
      base::ScopedFD fd,
      chromeos::VoidDBusMethodCallback callback) override;
  void Init(dbus::Bus* bus) override;

 private:
  raw_ptr<dbus::ObjectProxy, ExperimentalAsh> wilco_dtc_supportd_proxy_ =
      nullptr;

  base::WeakPtrFactory<WilcoDtcSupportdClientImpl> weak_ptr_factory_{this};
};

WilcoDtcSupportdClientImpl::WilcoDtcSupportdClientImpl() = default;

WilcoDtcSupportdClientImpl::~WilcoDtcSupportdClientImpl() = default;

void WilcoDtcSupportdClientImpl::WaitForServiceToBeAvailable(
    chromeos::WaitForServiceToBeAvailableCallback callback) {
  wilco_dtc_supportd_proxy_->WaitForServiceToBeAvailable(std::move(callback));
}

void WilcoDtcSupportdClientImpl::BootstrapMojoConnection(
    base::ScopedFD fd,
    chromeos::VoidDBusMethodCallback callback) {
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
  if (base::FeatureList::IsEnabled(::features::kWilcoDtc)) {
    (new WilcoDtcSupportdClientImpl())->Init(bus);
  }
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

}  // namespace ash
