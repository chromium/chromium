// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/payment_app_instance_ash.h"

#include <utility>

#include "ash/components/arc/pay/arc_payment_app_bridge.h"
#include "base/check.h"
#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/payments/core/native_error_strings.h"

namespace crosapi {

PaymentAppInstanceAsh::PaymentAppInstanceAsh() = default;

PaymentAppInstanceAsh::~PaymentAppInstanceAsh() = default;

void PaymentAppInstanceAsh::BindReceiver(
    mojo::PendingReceiver<chromeos::payments::mojom::PaymentAppInstance>
        receiver) {
  receivers_.Add(this, std::move(receiver));
}

void PaymentAppInstanceAsh::Initialize() {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  payment_app_service_ =
      arc::ArcPaymentAppBridge::GetForBrowserContext(profile);
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
  if (!payment_app_service_ || !parameters->request_token.has_value()) {
    std::move(callback).Run(
        chromeos::payments::mojom::InvokePaymentAppResult::NewError(
            payments::errors::kUnableToInvokeAndroidPaymentApps));
    return;
  }

  // TODO(https://crbug.com/1385989): Pass instance id over crosapi to find
  // the correct host window to put overlay on.

  payment_app_service_->InvokePaymentApp(std::move(parameters),
                                         std::move(callback));
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

}  // namespace crosapi
