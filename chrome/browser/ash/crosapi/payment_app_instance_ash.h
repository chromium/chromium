// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_PAYMENT_APP_INSTANCE_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_PAYMENT_APP_INSTANCE_ASH_H_

#include <string>

#include "base/sequence_checker.h"
#include "chromeos/components/payments/mojom/payment_app.mojom.h"
#include "chromeos/components/payments/mojom/payment_app_types.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace arc {
class ArcPaymentAppBridge;
}

namespace crosapi {

class PaymentAppInstanceAsh
    : public chromeos::payments::mojom::PaymentAppInstance {
 public:
  PaymentAppInstanceAsh();
  PaymentAppInstanceAsh(const PaymentAppInstanceAsh&) = delete;
  PaymentAppInstanceAsh& operator=(const PaymentAppInstanceAsh&) = delete;
  ~PaymentAppInstanceAsh() override;

  void BindReceiver(
      mojo::PendingReceiver<chromeos::payments::mojom::PaymentAppInstance>
          receiver);

  // Initialize the instance. Should only be called when
  // ProfileManager::GetPrimaryUserProfile() is ready to use.
  void Initialize();

  // mojom::PaymentAppInstance:
  void IsPaymentImplemented(const std::string& package_name,
                            IsPaymentImplementedCallback callback) override;
  void IsReadyToPay(chromeos::payments::mojom::PaymentParametersPtr parameters,
                    IsReadyToPayCallback callback) override;
  void InvokePaymentApp(
      chromeos::payments::mojom::PaymentParametersPtr parameters,
      InvokePaymentAppCallback callback) override;
  void AbortPaymentApp(const std::string& request_token,
                       AbortPaymentAppCallback callback) override;

  void SetPaymentAppServiceForTesting(
      arc::ArcPaymentAppBridge* payment_app_service);

 private:
  arc::ArcPaymentAppBridge* payment_app_service_ = nullptr;
  mojo::ReceiverSet<chromeos::payments::mojom::PaymentAppInstance> receivers_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_PAYMENT_APP_INSTANCE_ASH_H_
