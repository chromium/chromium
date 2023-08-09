// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_PAYMENT_APP_INSTANCE_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_PAYMENT_APP_INSTANCE_ASH_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chromeos/components/payments/mojom/payment_app.mojom.h"
#include "chromeos/components/payments/mojom/payment_app_types.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace arc {
class ArcPaymentAppBridge;
}

namespace apps {
class InstanceRegistry;
}

namespace crosapi {

class PaymentAppInstanceAsh
    : public chromeos::payments::mojom::PaymentAppInstance,
      public ProfileObserver {
 public:
  PaymentAppInstanceAsh();
  PaymentAppInstanceAsh(const PaymentAppInstanceAsh&) = delete;
  PaymentAppInstanceAsh& operator=(const PaymentAppInstanceAsh&) = delete;
  ~PaymentAppInstanceAsh() override;

  void BindReceiver(
      mojo::PendingReceiver<chromeos::payments::mojom::PaymentAppInstance>
          receiver);

  // Initialize the instance. Should only be called with a valid profile.
  void Initialize(Profile* profile);

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

  void SetInstanceRegistryForTesting(apps::InstanceRegistry* instance_registry);

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

 private:
  raw_ptr<arc::ArcPaymentAppBridge> payment_app_service_ = nullptr;
  raw_ptr<apps::InstanceRegistry> instance_registry_ = nullptr;
  mojo::ReceiverSet<chromeos::payments::mojom::PaymentAppInstance> receivers_;
  // Observe profile destruction to reset prefs observation.
  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_PAYMENT_APP_INSTANCE_ASH_H_
