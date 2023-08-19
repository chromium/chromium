// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/pay/arc_payment_app_bridge.h"

#include <utility>

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "base/functional/bind.h"
#include "base/memory/singleton.h"
#include "base/no_destructor.h"

namespace arc {
namespace {

static constexpr char kUnableToConnectErrorMessage[] =
    "Unable to invoke Android apps.";

class ArcPaymentAppBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcPaymentAppBridge,
          ArcPaymentAppBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcPaymentAppBridgeFactory";

  static ArcPaymentAppBridgeFactory* GetInstance() {
    static base::NoDestructor<ArcPaymentAppBridgeFactory> factory;
    return factory.get();
  }

  ArcPaymentAppBridgeFactory() = default;
  ~ArcPaymentAppBridgeFactory() override = default;

 private:
  friend base::DefaultSingletonTraits<ArcPaymentAppBridgeFactory>;
};

}  // namespace

// static
ArcPaymentAppBridge* ArcPaymentAppBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcPaymentAppBridgeFactory::GetForBrowserContext(context);
}

// static
ArcPaymentAppBridge* ArcPaymentAppBridge::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  return ArcPaymentAppBridgeFactory::GetForBrowserContextForTesting(context);
}

ArcPaymentAppBridge::ArcPaymentAppBridge(
    content::BrowserContext* browser_context,
    ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service) {}

ArcPaymentAppBridge::~ArcPaymentAppBridge() = default;

void ArcPaymentAppBridge::IsPaymentImplemented(
    const std::string& package_name,
    IsPaymentImplementedCallback callback) {
  chromeos::payments::mojom::PaymentAppInstance* payment_app =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->payment_app(),
                                  IsPaymentImplemented);
  if (!payment_app) {
    std::move(callback).Run(
        chromeos::payments::mojom::IsPaymentImplementedResult::NewError(
            kUnableToConnectErrorMessage));
    return;
  }

  payment_app->IsPaymentImplemented(package_name, std::move(callback));
}

void ArcPaymentAppBridge::IsReadyToPay(
    chromeos::payments::mojom::PaymentParametersPtr parameters,
    IsReadyToPayCallback callback) {
  chromeos::payments::mojom::PaymentAppInstance* payment_app =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->payment_app(),
                                  IsReadyToPay);
  if (!payment_app) {
    std::move(callback).Run(
        chromeos::payments::mojom::IsReadyToPayResult::NewError(
            kUnableToConnectErrorMessage));
    return;
  }

  payment_app->IsReadyToPay(std::move(parameters), std::move(callback));
}

void ArcPaymentAppBridge::InvokePaymentApp(
    chromeos::payments::mojom::PaymentParametersPtr parameters,
    InvokePaymentAppCallback callback) {
  chromeos::payments::mojom::PaymentAppInstance* payment_app =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->payment_app(),
                                  InvokePaymentApp);
  if (!payment_app) {
    std::move(callback).Run(
        chromeos::payments::mojom::InvokePaymentAppResult::NewError(
            kUnableToConnectErrorMessage));
    return;
  }

  payment_app->InvokePaymentApp(std::move(parameters), std::move(callback));
}

void ArcPaymentAppBridge::AbortPaymentApp(const std::string& request_token,
                                          AbortPaymentAppCallback callback) {
  chromeos::payments::mojom::PaymentAppInstance* payment_app =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->payment_app(),
                                  AbortPaymentApp);
  if (!payment_app) {
    std::move(callback).Run(false);
    return;
  }

  payment_app->AbortPaymentApp(request_token, std::move(callback));
}

// static
void ArcPaymentAppBridge::EnsureFactoryBuilt() {
  ArcPaymentAppBridgeFactory::GetInstance();
}

}  // namespace arc
