// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/payment_app_instance_ash.h"

#include <utility>

#include "ash/components/arc/pay/arc_payment_app_bridge.h"
#include "ash/public/cpp/external_arc/overlay/arc_overlay_manager.h"
#include "base/check.h"
#include "base/check_is_test.h"
#include "base/logging.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/payments/core/native_error_strings.h"
#include "components/services/app_service/public/cpp/instance_registry.h"
#include "content/public/browser/browser_thread.h"

namespace {

void OnInvokePaymentApp(
    crosapi::PaymentAppInstanceAsh::InvokePaymentAppCallback callback,
    base::OnceClosure overlay_state,
    chromeos::payments::mojom::InvokePaymentAppResultPtr response) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Dismiss and prevent any further overlays
  if (overlay_state) {
    std::move(overlay_state).Run();
  }

  std::move(callback).Run(std::move(response));
}

}  // namespace

namespace crosapi {

PaymentAppInstanceAsh::PaymentAppInstanceAsh() = default;

PaymentAppInstanceAsh::~PaymentAppInstanceAsh() = default;

void PaymentAppInstanceAsh::BindReceiver(
    mojo::PendingReceiver<chromeos::payments::mojom::PaymentAppInstance>
        receiver) {
  receivers_.Add(this, std::move(receiver));
}

void PaymentAppInstanceAsh::Initialize(Profile* profile) {
  CHECK(profile);
  // This method is called during crosapi binding, which could happen more than
  // once if Lacros instance is created more than once (e.g. crash and restart).
  if (profile_observation_.IsObservingSource(profile)) {
    VLOG(1) << "PaymentAppInstanceAsh is already initialized. Skip init.";
    return;
  }
  profile_observation_.Observe(profile);
  payment_app_service_ =
      arc::ArcPaymentAppBridge::GetForBrowserContext(profile);
  instance_registry_ =
      &apps::AppServiceProxyFactory::GetForProfile(profile)->InstanceRegistry();
}

void PaymentAppInstanceAsh::IsPaymentImplemented(
    const std::string& package_name,
    IsPaymentImplementedCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (package_name.empty()) {
    // Chrome OS supports Android app payment only through a TWA. An empty
    // `package_name` indicates that Chrome was not launched from a TWA,
    // so there're no payment apps available.
    std::move(callback).Run(
        chromeos::payments::mojom::IsPaymentImplementedResult::NewValid(
            chromeos::payments::mojom::IsPaymentImplementedValidResult::New()));
    return;
  }

  if (!payment_app_service_) {
    std::move(callback).Run(
        chromeos::payments::mojom::IsPaymentImplementedResult::NewError(
            payments::errors::kUnableToInvokeAndroidPaymentApps));
    return;
  }

  payment_app_service_->IsPaymentImplemented(package_name, std::move(callback));
}

void PaymentAppInstanceAsh::IsReadyToPay(
    chromeos::payments::mojom::PaymentParametersPtr parameters,
    IsReadyToPayCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!payment_app_service_) {
    std::move(callback).Run(
        chromeos::payments::mojom::IsReadyToPayResult::NewError(
            payments::errors::kUnableToInvokeAndroidPaymentApps));
    return;
  }

  payment_app_service_->IsReadyToPay(std::move(parameters),
                                     std::move(callback));
}

void PaymentAppInstanceAsh::InvokePaymentApp(
    chromeos::payments::mojom::PaymentParametersPtr parameters,
    InvokePaymentAppCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!payment_app_service_ || !parameters->request_token.has_value() ||
      !parameters->twa_instance_identifier.has_value() || !instance_registry_) {
    std::move(callback).Run(
        chromeos::payments::mojom::InvokePaymentAppResult::NewError(
            payments::errors::kUnableToInvokeAndroidPaymentApps));
    return;
  }

  ash::ArcOverlayManager* const overlay_manager =
      ash::ArcOverlayManager::instance();

  aura::Window* host_window = nullptr;

  instance_registry_->ForOneInstance(
      parameters->twa_instance_identifier.value(),
      [&host_window](const apps::InstanceUpdate& update) {
        host_window = update.Window();
      });

  if (!host_window) {
    std::move(callback).Run(
        chromeos::payments::mojom::InvokePaymentAppResult::NewError(
            payments::errors::kUnableToInvokeAndroidPaymentApps));
    return;
  }

  base::OnceClosure overlay_state =
      overlay_manager
          ->RegisterHostWindow(parameters->request_token.value(), host_window)
          .Release();

  payment_app_service_->InvokePaymentApp(
      std::move(parameters),
      base::BindOnce(&OnInvokePaymentApp, std::move(callback),
                     std::move(overlay_state)));
}

void PaymentAppInstanceAsh::AbortPaymentApp(const std::string& request_token,
                                            AbortPaymentAppCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!payment_app_service_) {
    std::move(callback).Run(false);
    return;
  }

  payment_app_service_->AbortPaymentApp(request_token, std::move(callback));
}

void PaymentAppInstanceAsh::SetPaymentAppServiceForTesting(
    arc::ArcPaymentAppBridge* payment_app_service) {
  payment_app_service_ = payment_app_service;
}

void PaymentAppInstanceAsh::SetInstanceRegistryForTesting(
    apps::InstanceRegistry* instance_registry) {
  instance_registry_ = instance_registry;
}

void PaymentAppInstanceAsh::OnProfileWillBeDestroyed(Profile* profile) {
  payment_app_service_ = nullptr;
  instance_registry_ = nullptr;
  profile_observation_.Reset();
}

}  // namespace crosapi
