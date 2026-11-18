// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_actuator/local_actuation/local_actuation_peer_verifier.h"

#include <bsm/libbsm.h>

#include <cstdint>
#include <optional>

#include "base/containers/span.h"
#include "base/mac/process_requirement.h"
#include "build/branding_buildflags.h"

namespace browser_actuator {

namespace {

std::optional<base::mac::ProcessRequirement> BuildPeerRequirement() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return base::mac::ProcessRequirement::Builder()
      .SignedWithSameIdentity()
      .IdentifierIsOneOf(
          {"com.google.GeminiMacOS", "com.google.GeminiMacOS.dev"})
      .CheckDynamicValidityOnly()
      .Build();
#else
  return std::nullopt;
#endif
}

}  // namespace

LocalActuationPeerVerifier::LocalActuationPeerVerifier()
    : requirement_(BuildPeerRequirement()) {}

LocalActuationPeerVerifier::~LocalActuationPeerVerifier() = default;

bool LocalActuationPeerVerifier::Verify(
    audit_token_t audit_token,
    base::span<const uint8_t> info_plist) const {
  // `ValidateProcess()` CHECKs that `info_plist` is not empty.
  if (info_plist.empty() || !requirement_.has_value()) {
    return false;
  }
  return requirement_->ValidateProcess(audit_token, info_plist);
}

}  // namespace browser_actuator
