// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_INVALIDATION_FAKE_AFFILIATED_INVALIDATION_SERVICE_PROVIDER_H_
#define CHROME_BROWSER_ASH_POLICY_INVALIDATION_FAKE_AFFILIATED_INVALIDATION_SERVICE_PROVIDER_H_

#include "chrome/browser/ash/policy/invalidation/affiliated_invalidation_service_provider.h"

namespace policy {

class FakeAffiliatedInvalidationServiceProvider
    : public AffiliatedInvalidationServiceProvider {
 public:
  FakeAffiliatedInvalidationServiceProvider();

  FakeAffiliatedInvalidationServiceProvider(
      const FakeAffiliatedInvalidationServiceProvider&) = delete;
  FakeAffiliatedInvalidationServiceProvider& operator=(
      const FakeAffiliatedInvalidationServiceProvider&) = delete;

  // AffiliatedInvalidationServiceProvider:
  void RegisterConsumer(Consumer* consumer) override;
  void UnregisterConsumer(Consumer* consumer) override;
  void Shutdown() override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_INVALIDATION_FAKE_AFFILIATED_INVALIDATION_SERVICE_PROVIDER_H_
