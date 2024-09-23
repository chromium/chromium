// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations_mixin.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/scoped_privacy_sandbox_attestations.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"

namespace privacy_sandbox {

PrivacySandboxAttestationsMixin::PrivacySandboxAttestationsMixin(
    InProcessBrowserTestMixinHost* host)
    : InProcessBrowserTestMixin(host) {
  // TODO(crbug.com/41483922): Once Privacy Sandbox is back to default-deny
  // behavior, remove feature `kDefaultAllowPrivacySandboxAttestations`.
  scoped_feature_list_.InitAndDisableFeature(
      kDefaultAllowPrivacySandboxAttestations);
}

PrivacySandboxAttestationsMixin::~PrivacySandboxAttestationsMixin() = default;

void PrivacySandboxAttestationsMixin::SetUpOnMainThread() {
  // `PrivacySandboxAttestations` has a member of type
  // `scoped_refptr<base::SequencedTaskRunner>`, so it must be initialized after
  // a browser process is created.
  scoped_attestations_ = std::make_unique<ScopedPrivacySandboxAttestations>(
      PrivacySandboxAttestations::CreateForTesting());
}

}  // namespace privacy_sandbox
