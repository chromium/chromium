// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/test/arc_payment_app_bridge_test_support.h"

#include "ash/components/arc/session/arc_bridge_service.h"

namespace arc {

ArcPaymentAppBridgeTestSupport::ScopedSetInstance::ScopedSetInstance(
    ArcServiceManager* manager,
    chromeos::payments::mojom::PaymentAppInstance* instance)
    : manager_(manager), instance_(instance) {
  manager_->arc_bridge_service()->payment_app()->SetInstance(instance_.get());
}

ArcPaymentAppBridgeTestSupport::ScopedSetInstance::~ScopedSetInstance() {
  manager_->arc_bridge_service()->payment_app()->CloseInstance(instance_.get());
}

ArcPaymentAppBridgeTestSupport::ArcPaymentAppBridgeTestSupport() = default;

ArcPaymentAppBridgeTestSupport::~ArcPaymentAppBridgeTestSupport() = default;

std::unique_ptr<ArcPaymentAppBridgeTestSupport::ScopedSetInstance>
ArcPaymentAppBridgeTestSupport::CreateScopedSetInstance() {
  return std::make_unique<ScopedSetInstance>(manager(), instance());
}

}  // namespace arc
