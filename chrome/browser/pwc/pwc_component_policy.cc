// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pwc/pwc_component_policy.h"

#include <algorithm>
#include <utility>

#include "base/check.h"
#include "base/notreached.h"
#include "url/url_constants.h"

namespace pwc {

namespace {

PwcComponentPolicy::NewWindowPolicy NewWindowPolicyForComponent(
    PrivilegedComponent component) {
  switch (component) {
    case PrivilegedComponent::kTestComponent:
      return PwcComponentPolicy::NewWindowPolicy::kDrop;
    case PrivilegedComponent::kGlic:
      return PwcComponentPolicy::NewWindowPolicy::kOpenAsUnrelatedTab;
  }
  NOTREACHED();
}

}  // namespace

FixedPwcPolicyDelegate::FixedPwcPolicyDelegate(
    std::vector<url::Origin> navigation_allowlist,
    std::vector<url::Origin> capability_allowlist)
    : navigation_allowlist_(std::move(navigation_allowlist)),
      capability_allowlist_(std::move(capability_allowlist)) {}

FixedPwcPolicyDelegate::~FixedPwcPolicyDelegate() = default;

bool FixedPwcPolicyDelegate::IsNavigationAllowed(
    const url::Origin& origin) const {
  return std::ranges::contains(navigation_allowlist_, origin);
}

bool FixedPwcPolicyDelegate::IsCapabilityOrigin(
    const url::Origin& origin) const {
  return std::ranges::contains(capability_allowlist_, origin);
}

PwcComponentPolicy::PwcComponentPolicy(
    PrivilegedComponent component,
    std::unique_ptr<PwcPolicyDelegate> delegate)
    : component_(component),
      delegate_(std::move(delegate)),
      new_window_policy_(NewWindowPolicyForComponent(component)),
      content_(ContentEnforcementForComponent(component)) {
  CHECK(delegate_);
}

PwcComponentPolicy::~PwcComponentPolicy() = default;

// static
PwcComponentPolicy::ContentEnforcement
PwcComponentPolicy::ContentEnforcementForComponent(
    PrivilegedComponent component) {
  ContentEnforcement enforcement;
  // Derive the process-grouping id from the enum value itself, so it is
  // distinct per component by construction and there are no hand-written ids
  // to mistype or duplicate.
  enforcement.feature_id = static_cast<int32_t>(component);
  switch (component) {
    case PrivilegedComponent::kTestComponent:
      // Asymmetric bits so tests catch a swapped-field mapping bug.
      enforcement.disallow_service_worker_control = true;
      enforcement.disallow_shared_workers = false;
      return enforcement;
    case PrivilegedComponent::kGlic:
      enforcement.disallow_service_worker_control = true;
      enforcement.disallow_shared_workers = true;
      return enforcement;
  }
  NOTREACHED();
}

bool PwcComponentPolicy::IsNavigationAllowed(const url::Origin& origin) const {
  // Structural guardrail: only secure HTTPS origins may ever be blessed,
  // regardless of what the delegate answers.
  if (origin.opaque() || origin.scheme() != url::kHttpsScheme) {
    return false;
  }
  return delegate_->IsNavigationAllowed(origin);
}

bool PwcComponentPolicy::IsCapabilityOrigin(const url::Origin& origin) const {
  // Structural guardrail: capability is a refinement of navigation. A
  // delegate cannot grant capability to an origin the main frame could
  // never commit.
  return IsNavigationAllowed(origin) && delegate_->IsCapabilityOrigin(origin);
}

}  // namespace pwc
