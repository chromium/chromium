// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PWC_PWC_COMPONENT_POLICY_H_
#define CHROME_BROWSER_PWC_PWC_COMPONENT_POLICY_H_

#include <cstdint>
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

  // Fixed per-component content-layer enforcement bits, mirrored into
  // content::WebContents::PrivilegedParams when the WebContents is created.
  // These are security-sensitive (see the file's OWNERS): a component that
  // permits service worker control or shares a feature id is a security
  // downgrade, so they live in this security-reviewed file rather than at the
  // creation site.
  //
  // Opaque per-component process-grouping id, derived from the
  // PrivilegedComponent value so it is distinct per component by
  // construction. WebContents with the same id may share a renderer process
  // with each other -- still subject to site isolation, so only different
  // instances of the same site actually coalesce -- but never share a process
  // with ordinary WebContents.
  int32_t content_feature_id() const { return content_.feature_id; }
  bool disallow_service_worker_control() const {
    return content_.disallow_service_worker_control;
  }
  bool disallow_shared_workers() const {
    return content_.disallow_shared_workers;
  }

  // True iff the primary main frame may commit `origin`: `origin` is HTTPS
  // and the delegate allows it.
  bool IsNavigationAllowed(const url::Origin& origin) const;

  // True iff a primary main frame committed to `origin` may receive the
  // elevated capability bridge: IsNavigationAllowed(origin) and the delegate
  // grants capability. Structurally implies IsNavigationAllowed(origin).
  bool IsCapabilityOrigin(const url::Origin& origin) const;

 private:
  struct ContentEnforcement {
    int32_t feature_id = 0;
    bool disallow_service_worker_control = false;
    bool disallow_shared_workers = false;
  };

  static ContentEnforcement ContentEnforcementForComponent(
      PrivilegedComponent component);

  const PrivilegedComponent component_;
  const std::unique_ptr<PwcPolicyDelegate> delegate_;
  const NewWindowPolicy new_window_policy_;
  const ContentEnforcement content_;
};

}  // namespace pwc

#endif  // CHROME_BROWSER_PWC_PWC_COMPONENT_POLICY_H_
