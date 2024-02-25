// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_TEST_ARC_PAYMENT_APP_BRIDGE_TEST_SUPPORT_H_
#define ASH_COMPONENTS_ARC_TEST_ARC_PAYMENT_APP_BRIDGE_TEST_SUPPORT_H_

#include <memory>
#include <string>

#include "ash/components/arc/pay/arc_payment_app_bridge.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/components/payments/mock_payment_app_instance.h"
#include "chromeos/components/payments/mojom/payment_app.mojom.h"
#include "chromeos/components/payments/mojom/payment_app_types.mojom.h"
#include "components/user_prefs/test/test_browser_context_with_prefs.h"
#include "content/public/test/browser_task_environment.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

// Common support utility for tests of payment_app.mojom interface.
class ArcPaymentAppBridgeTestSupport {
 public:
  // Sets up the payment_app.mojom connection in the constructor and disconnects
  // in the destructor.
  class ScopedSetInstance {
   public:
    ScopedSetInstance(ArcServiceManager* manager,
                      chromeos::payments::mojom::PaymentAppInstance* instance);
    ~ScopedSetInstance();

    ScopedSetInstance(const ScopedSetInstance& other) = delete;
    ScopedSetInstance& operator=(const ScopedSetInstance& other) = delete;

   private:
    raw_ptr<ArcServiceManager> manager_;
    raw_ptr<chromeos::payments::mojom::PaymentAppInstance> instance_;
  };

  ArcPaymentAppBridgeTestSupport();
  ~ArcPaymentAppBridgeTestSupport();

  ArcPaymentAppBridgeTestSupport(const ArcPaymentAppBridgeTestSupport& other) =
      delete;
  ArcPaymentAppBridgeTestSupport& operator=(
      const ArcPaymentAppBridgeTestSupport& other) = delete;

  // Creates a ScopedSetInstance to be placed on the stack, so that
  // payment_app.mojom connection is available in the test and is cleaned up
  // when the returned value goes out of scope.
  std::unique_ptr<ScopedSetInstance> CreateScopedSetInstance();

  // The ARC service manager.
  ArcServiceManager* manager() { return ArcServiceManager::Get(); }

  // The mock payment_app.mojom connection.
  payments::MockPaymentAppInstance* instance() { return &instance_; }

  // The browser context that should be used in the test.
  content::BrowserContext* context() { return &context_; }

 private:
  // Required for the TestBrowserContext and ArcServiceManager.
  content::BrowserTaskEnvironment task_environment_;

  // Used for retrieving an instance of ArcPaymentAppBridge owned by a
  // BrowserContext.
  user_prefs::TestBrowserContextWithPrefs context_;

  // The unit test must create an instance of ArcServiceManager for
  // ArcServiceManager::Get() to work correctly.
  ArcServiceManager manager_;

  payments::MockPaymentAppInstance instance_;
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_TEST_ARC_PAYMENT_APP_BRIDGE_TEST_SUPPORT_H_
