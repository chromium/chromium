// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PWC_PWC_COMPONENT_POLICY_H_
#define CHROME_BROWSER_PWC_PWC_COMPONENT_POLICY_H_

#include <memory>
#include <vector>

#include "url/origin.h"

namespace pwc {

// Identifies the blessed component a PrivilegedWebContents serves. Adding
// a value requires security review (see OWNERS).
enum class PrivilegedComponent {
  // Test-only component.
  kTestComponent,
  // The glic panel (gemini.google.com).
  kGlic,
};

// Supplies a component's origin allowlists. Implemented by the blessed
// component — either in this directory or in the component's own — so its
// owner can control the lists dynamically (feature params, enterprise
// policy, server-provided configuration, ...). Implementations are
// security-sensitive: keep them in files covered by a SECURITY_OWNERS
// per-file rule (as pwc_component_policy.* is here; implementations in
// other directories should add an equivalent rule).
//
// Delegates answer inside structural guardrails enforced by
// PwcComponentPolicy: a non-HTTPS origin is denied before the delegate is
// consulted, and capability additionally requires navigability. A delegate
// bug can therefore narrow the effective policy but never widen it beyond
// HTTPS origins, and never grant capability to a non-navigable origin.
//
// Two-tier trust model: navigation is the set of origins the primary main
// frame may ever commit; capability is the subset of those origins that
// additionally receive the elevated capability bridge. A main frame
// committed to a navigation-only origin runs in the isolated process but is
// never handed capabilities.
class PwcPolicyDelegate {
 public:
  virtual ~PwcPolicyDelegate() = default;

  // True iff the primary main frame may commit `origin`.
  virtual bool IsNavigationAllowed(const url::Origin& origin) const = 0;

  // True iff a primary main frame committed to `origin` may receive the
  // elevated capability bridge.
  virtual bool IsCapabilityOrigin(const url::Origin& origin) const = 0;
};

// A PwcPolicyDelegate that answers from fixed origin lists. Suitable for
// tests and for components whose allowlists do not change at runtime.
class FixedPwcPolicyDelegate : public PwcPolicyDelegate {
 public:
  FixedPwcPolicyDelegate(std::vector<url::Origin> navigation_allowlist,
                         std::vector<url::Origin> capability_allowlist);
  ~FixedPwcPolicyDelegate() override;

  bool IsNavigationAllowed(const url::Origin& origin) const override;
  bool IsCapabilityOrigin(const url::Origin& origin) const override;

 private:
  const std::vector<url::Origin> navigation_allowlist_;
  const std::vector<url::Origin> capability_allowlist_;
};

// The policy a PrivilegedWebContents enforces: the component identity and
// its fixed per-component behaviors, plus the component-supplied delegate
// for the dynamic allowlists, wrapped in the structural guardrails
// described on PwcPolicyDelegate.
class PwcComponentPolicy {
 public:
  enum class NewWindowPolicy {
    // Window-creation requests from the PWC are dropped.
    kDrop,
    // The requested URL is re-dispatched as an ordinary, unrelated
    // (noopener) foreground tab. The PWC never gets a related window.
    kOpenAsUnrelatedTab,
  };

  PwcComponentPolicy(PrivilegedComponent component,
                     std::unique_ptr<PwcPolicyDelegate> delegate);
  PwcComponentPolicy(const PwcComponentPolicy&) = delete;
  PwcComponentPolicy& operator=(const PwcComponentPolicy&) = delete;
  ~PwcComponentPolicy();

  PrivilegedComponent component() const { return component_; }
  NewWindowPolicy new_window_policy() const { return new_window_policy_; }

  // True iff the primary main frame may commit `origin`: `origin` is HTTPS
  // and the delegate allows it.
  bool IsNavigationAllowed(const url::Origin& origin) const;

  // True iff a primary main frame committed to `origin` may receive the
  // elevated capability bridge: IsNavigationAllowed(origin) and the delegate
  // grants capability. Structurally implies IsNavigationAllowed(origin).
  bool IsCapabilityOrigin(const url::Origin& origin) const;

 private:
  const PrivilegedComponent component_;
  const std::unique_ptr<PwcPolicyDelegate> delegate_;
  const NewWindowPolicy new_window_policy_;
};

}  // namespace pwc

#endif  // CHROME_BROWSER_PWC_PWC_COMPONENT_POLICY_H_
