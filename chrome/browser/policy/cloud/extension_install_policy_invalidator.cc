// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/extension_install_policy_invalidator.h"

#include <memory>

#include "base/task/sequenced_task_runner.h"
#include "components/invalidation/invalidation_listener.h"
#include "components/policy/core/common/cloud/cloud_policy_refresh_scheduler.h"
#include "components/policy/core/common/cloud/enterprise_metrics.h"
#include "components/policy/core/common/cloud/policy_invalidation_util.h"
#include "components/policy/policy_constants.h"

namespace policy {

namespace {

constexpr char kDevicePolicyInvalidatorTypeName[] =
    "EXTENSION_INSTALL_POLICY_FETCH";
constexpr char kBrowserPolicyInvalidatorTypeName[] =
    "EXTENSION_INSTALL_POLICY_FETCH";
constexpr char kUserPolicyInvalidatorTypeName[] =
    "EXTENSION_INSTALL_POLICY_FETCH";

}  // namespace

// static
const char* ExtensionInstallPolicyInvalidator::GetPolicyRefreshMetricName(
    PolicyInvalidationScope scope) {
  switch (scope) {
    case PolicyInvalidationScope::kUser:
      return kMetricUserExtensionInstallPolicyRefresh;
    case PolicyInvalidationScope::kDevice:
      return kMetricDeviceExtensionInstallPolicyRefresh;
    case PolicyInvalidationScope::kCBCM:
      return kMetricCBCMExtensionInstallPolicyRefresh;
    case PolicyInvalidationScope::kDeviceLocalAccount:
      NOTREACHED();
  }
}

// static
const char* ExtensionInstallPolicyInvalidator::GetPolicyInvalidationMetricName(
    PolicyInvalidationScope scope) {
  switch (scope) {
    case PolicyInvalidationScope::kUser:
      return kMetricUserExtensionInstallPolicyInvalidations;
    case PolicyInvalidationScope::kDevice:
      return kMetricDeviceExtensionInstallPolicyInvalidations;
    case PolicyInvalidationScope::kCBCM:
      return kMetricCBCMExtensionInstallPolicyInvalidations;
    case PolicyInvalidationScope::kDeviceLocalAccount:
      NOTREACHED();
  }
}

ExtensionInstallPolicyInvalidator::ExtensionInstallPolicyInvalidator(
    PolicyInvalidationScope scope,
    invalidation::InvalidationListener* invalidation_listener,
    CloudPolicyCore* core,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const base::Clock* clock,
    const std::string& device_local_account_id)
    : PolicyInvalidator(
          scope,
          invalidation_listener,
          core,
          clock,
          device_local_account_id,
          std::make_unique<ExtensionInstallPolicyInvalidator::
                               ExtensionInstallPolicyInvalidationHandler>(
              scope,
              core,
              clock,
              std::move(task_runner))) {
  // This needs to be in the derived class because down the line, virtual
  // functions may be called and we need them to use the derived class
  // definition and not the base class one.
  core_observation_.Observe(core_);
  if (core_->refresh_scheduler()) {
    OnRefreshSchedulerStarted(core_);
  }
}

ExtensionInstallPolicyInvalidator::~ExtensionInstallPolicyInvalidator() =
    default;

std::string ExtensionInstallPolicyInvalidator::GetType() const {
  switch (scope_) {
    case PolicyInvalidationScope::kUser:
      return kUserPolicyInvalidatorTypeName;
    case PolicyInvalidationScope::kDevice:
      return kDevicePolicyInvalidatorTypeName;
    case PolicyInvalidationScope::kCBCM:
      return kBrowserPolicyInvalidatorTypeName;
    case PolicyInvalidationScope::kDeviceLocalAccount:
      NOTREACHED();
  }
}

ExtensionInstallPolicyInvalidator::ExtensionInstallPolicyInvalidationHandler::
    ExtensionInstallPolicyInvalidationHandler(
        PolicyInvalidationScope scope,
        CloudPolicyCore* core,
        const base::Clock* clock,
        scoped_refptr<base::SequencedTaskRunner> task_runner)
    : PolicyInvalidator::PolicyInvalidationHandler(scope,
                                                   core,
                                                   clock,
                                                   task_runner) {}

ExtensionInstallPolicyInvalidator::ExtensionInstallPolicyInvalidationHandler::
    ~ExtensionInstallPolicyInvalidationHandler() = default;

const char* ExtensionInstallPolicyInvalidator::
    ExtensionInstallPolicyInvalidationHandler::GetPolicyRefreshMetricName(
        PolicyInvalidationScope scope) {
  return ExtensionInstallPolicyInvalidator::GetPolicyRefreshMetricName(scope);
}

const char* ExtensionInstallPolicyInvalidator::
    ExtensionInstallPolicyInvalidationHandler::GetPolicyInvalidationMetricName(
        PolicyInvalidationScope scope) {
  return ExtensionInstallPolicyInvalidator::GetPolicyInvalidationMetricName(
      scope);
}

}  // namespace policy
