// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/cloud_policy_invalidator.h"

#include <memory>

#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "components/invalidation/invalidation_listener.h"
#include "components/policy/core/common/cloud/cloud_policy_refresh_scheduler.h"
#include "components/policy/core/common/cloud/enterprise_metrics.h"
#include "components/policy/core/common/cloud/policy_invalidation_util.h"
#include "components/policy/policy_constants.h"
namespace policy {

namespace {

constexpr char kDevicePolicyInvalidatorTypeName[] = "DEVICE_POLICY_FETCH";
constexpr char kBrowserPolicyInvalidatorTypeName[] = "BROWSER_POLICY_FETCH";
constexpr char kUserPolicyInvalidatorTypeName[] = "USER_POLICY_FETCH";
constexpr char kDeviceLocalAccountPolicyInvalidatorTypeNameTemplate[] =
    "PUBLIC_ACCOUNT_POLICY_FETCH-%s";

}  // namespace

// static
const char* CloudPolicyInvalidator::GetPolicyRefreshMetricName(
    PolicyInvalidationScope scope) {
  switch (scope) {
    case PolicyInvalidationScope::kUser:
      return kMetricUserPolicyRefresh;
    case PolicyInvalidationScope::kDevice:
      return kMetricDevicePolicyRefresh;
    case PolicyInvalidationScope::kDeviceLocalAccount:
      return kMetricDeviceLocalAccountPolicyRefresh;
    case PolicyInvalidationScope::kCBCM:
      return kMetricCBCMPolicyRefresh;
  }
}

// static
const char* CloudPolicyInvalidator::GetPolicyInvalidationMetricName(
    PolicyInvalidationScope scope) {
  switch (scope) {
    case PolicyInvalidationScope::kUser:
      return kMetricUserPolicyInvalidations;
    case PolicyInvalidationScope::kDevice:
      return kMetricDevicePolicyInvalidations;
    case PolicyInvalidationScope::kDeviceLocalAccount:
      return kMetricDeviceLocalAccountPolicyInvalidations;
    case PolicyInvalidationScope::kCBCM:
      return kMetricCBCMPolicyInvalidations;
  }
}

CloudPolicyInvalidator::CloudPolicyInvalidator(
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
          std::make_unique<CloudPolicyInvalidationHandler>(scope,
                                                           core,
                                                           clock,
                                                           task_runner)) {
  // This needs to be in the derived class because down the line, virtual
  // functions may be called and we need them to use the derived class
  // definition and not the base class one.
  core_observation_.Observe(core_);
  if (core_->refresh_scheduler()) {
    OnRefreshSchedulerStarted(core_);
  }
}

CloudPolicyInvalidator::~CloudPolicyInvalidator() = default;

std::string CloudPolicyInvalidator::GetType() const {
  switch (scope_) {
    case PolicyInvalidationScope::kUser:
      return kUserPolicyInvalidatorTypeName;
    case PolicyInvalidationScope::kDevice:
      return kDevicePolicyInvalidatorTypeName;
    case PolicyInvalidationScope::kCBCM:
      return kBrowserPolicyInvalidatorTypeName;
    case PolicyInvalidationScope::kDeviceLocalAccount:
      return base::StringPrintf(
          kDeviceLocalAccountPolicyInvalidatorTypeNameTemplate,
          device_local_account_id_.c_str());
  }
}

CloudPolicyInvalidator::CloudPolicyInvalidationHandler::
    CloudPolicyInvalidationHandler(
        PolicyInvalidationScope scope,
        CloudPolicyCore* core,
        const base::Clock* clock,
        scoped_refptr<base::SequencedTaskRunner> task_runner)
    : PolicyInvalidationHandler(scope, core, clock, task_runner) {}

CloudPolicyInvalidator::CloudPolicyInvalidationHandler::
    ~CloudPolicyInvalidationHandler() = default;

const char* CloudPolicyInvalidator::CloudPolicyInvalidationHandler::
    GetPolicyRefreshMetricName(PolicyInvalidationScope scope) {
  return CloudPolicyInvalidator::GetPolicyRefreshMetricName(scope);
}

const char* CloudPolicyInvalidator::CloudPolicyInvalidationHandler::
    GetPolicyInvalidationMetricName(PolicyInvalidationScope scope) {
  return CloudPolicyInvalidator::GetPolicyInvalidationMetricName(scope);
}

}  // namespace policy
