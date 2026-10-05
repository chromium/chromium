// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GLIC_PWC_POLICY_DELEGATE_H_
#define CHROME_BROWSER_GLIC_HOST_GLIC_PWC_POLICY_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/pwc/pwc_component_policy.h"

class Profile;

namespace url {
class Origin;
}

namespace glic {

// PwcPolicyDelegate implementation for Glic in GlicNoWebview mode. Answers
// navigation and capability questions from the guest origin allowlist, and
// opts HTTP origins in to navigation under --glic-dev.
class GlicPwcPolicyDelegate : public pwc::PwcPolicyDelegate {
 public:
  explicit GlicPwcPolicyDelegate(Profile* profile);
  ~GlicPwcPolicyDelegate() override;

  GlicPwcPolicyDelegate(const GlicPwcPolicyDelegate&) = delete;
  GlicPwcPolicyDelegate& operator=(const GlicPwcPolicyDelegate&) = delete;

  // pwc::PwcPolicyDelegate:
  bool IsNavigationAllowed(const url::Origin& origin) const override;
  bool IsCapabilityOrigin(const url::Origin& origin) const override;
  bool AllowsInsecureDevOrigin(const url::Origin& origin) const override;

 private:
  const raw_ptr<Profile> profile_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GLIC_PWC_POLICY_DELEGATE_H_
