// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PWC_PRIVILEGED_WEB_CONTENTS_H_
#define CHROME_BROWSER_PWC_PRIVILEGED_WEB_CONTENTS_H_

#include <memory>

#include "chrome/browser/pwc/pwc_component_policy.h"
#include "content/public/browser/web_contents_delegate.h"

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace pwc {

// Owns a WebContents that hosts remote content for a blessed component with
// elevated browser capabilities.
//
// Structural properties (see the PWC design doc):
// - A PrivilegedWebContents is not a tab and can never become one: it owns
//   its WebContents exclusively and exposes no ownership-transfer API.
// - The WebContents lives in the profile's default StoragePartition, so the
//   component shares the live cookie jar with ordinary browsing.
// - PrivilegedWebContents is the WebContentsDelegate for its WebContents.
//
// This is the skeleton: enforcement (process isolation, service worker
// controls, navigation policy, capability bridge) is added by later CLs.
class PrivilegedWebContents : public content::WebContentsDelegate {
 public:
  // Creates a PrivilegedWebContents for `component` in `browser_context`.
  // `policy_delegate` supplies the component's origin allowlists and is
  // implemented by the component owner (see PwcPolicyDelegate); it is
  // consulted through the structural guardrails of PwcComponentPolicy. Must
  // only be called when pwc::mojom::features::kPrivilegedWebContents is
  // enabled.
  static std::unique_ptr<PrivilegedWebContents> Create(
      PrivilegedComponent component,
      content::BrowserContext* browser_context,
      std::unique_ptr<PwcPolicyDelegate> policy_delegate);

  // Returns the owning PrivilegedWebContents if `web_contents` is owned by
  // one, otherwise nullptr.
  static PrivilegedWebContents* FromWebContents(
      content::WebContents* web_contents);

  PrivilegedWebContents(const PrivilegedWebContents&) = delete;
  PrivilegedWebContents& operator=(const PrivilegedWebContents&) = delete;
  ~PrivilegedWebContents() override;

  content::WebContents* web_contents() { return web_contents_.get(); }
  const PwcComponentPolicy& policy() const { return policy_; }
  PrivilegedComponent component() const { return policy_.component(); }

 private:
  PrivilegedWebContents(PrivilegedComponent component,
                        content::BrowserContext* browser_context,
                        std::unique_ptr<PwcPolicyDelegate> policy_delegate);

  const PwcComponentPolicy policy_;
  std::unique_ptr<content::WebContents> web_contents_;
};

}  // namespace pwc

#endif  // CHROME_BROWSER_PWC_PRIVILEGED_WEB_CONTENTS_H_
