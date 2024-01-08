// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_AFFILIATED_REMOTE_COMMANDS_INVALIDATOR_H_
#define CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_AFFILIATED_REMOTE_COMMANDS_INVALIDATOR_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/policy/invalidation/affiliated_invalidation_service_provider.h"
#include "components/policy/core/common/cloud/policy_invalidation_scope.h"

namespace policy {

class CloudPolicyCore;
class RemoteCommandsInvalidatorImpl;

// This is a wrapper class to be used for device commands and device-local
// account commands.
class AffiliatedRemoteCommandsInvalidator
    : public AffiliatedInvalidationServiceProvider::Consumer {
 public:
  AffiliatedRemoteCommandsInvalidator(
      CloudPolicyCore* core,
      AffiliatedInvalidationServiceProvider* invalidation_service_provider,
      PolicyInvalidationScope scope);

  AffiliatedRemoteCommandsInvalidator(
      const AffiliatedRemoteCommandsInvalidator&) = delete;
  AffiliatedRemoteCommandsInvalidator& operator=(
      const AffiliatedRemoteCommandsInvalidator&) = delete;

  ~AffiliatedRemoteCommandsInvalidator() override;

  // AffiliatedInvalidationServiceProvider::Consumer:
  void OnInvalidationServiceSet(
      invalidation::InvalidationService* invalidation_service) override;

 private:
  const raw_ptr<CloudPolicyCore> core_;
  const raw_ptr<AffiliatedInvalidationServiceProvider>
      invalidation_service_provider_;

  std::unique_ptr<RemoteCommandsInvalidatorImpl> invalidator_;

  const PolicyInvalidationScope scope_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_AFFILIATED_REMOTE_COMMANDS_INVALIDATOR_H_
