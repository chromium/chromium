// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_pwc_policy_delegate.h"

#include "base/command_line.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "url/origin.h"

namespace glic {

GlicPwcPolicyDelegate::GlicPwcPolicyDelegate(Profile* profile)
    : profile_(profile) {}

GlicPwcPolicyDelegate::~GlicPwcPolicyDelegate() = default;

bool GlicPwcPolicyDelegate::IsNavigationAllowed(
    const url::Origin& origin) const {
  return IsGuestOriginAllowed(origin, profile_);
}

bool GlicPwcPolicyDelegate::IsCapabilityOrigin(
    const url::Origin& origin) const {
  return IsOriginAllowedGlicApi(origin, profile_);
}

// Under --glic-dev, IsGuestOriginAllowed() already accepts any origin; let the
// PWC HTTPS guardrail follow suit so HTTP dev servers can load.
bool GlicPwcPolicyDelegate::AllowsInsecureDevOrigin(
    const url::Origin& origin) const {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ::switches::kGlicDev);
}

}  // namespace glic
