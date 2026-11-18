// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_ACTUATOR_LOCAL_ACTUATION_LOCAL_ACTUATION_PEER_VERIFIER_H_
#define CHROME_BROWSER_BROWSER_ACTUATOR_LOCAL_ACTUATION_LOCAL_ACTUATION_PEER_VERIFIER_H_

#include <bsm/libbsm.h>

#include <cstdint>
#include <optional>

#include "base/containers/span.h"
#include "base/mac/process_requirement.h"
#include "chrome/browser/browser_actuator/local_actuation/peer_verifier.h"

namespace browser_actuator {

// Accepts a sender only if it is Gemini for macOS and is signed with the same
// identity as this browser. Validates the code the sender is running, not its
// bundle on disk, so `Verify()` needs the sender's `Info.plist`.
//
// Accepts every sender when this browser is unsigned or ad-hoc signed, so that
// local builds can be tested. Rejects every sender in builds without Google
// Chrome branding.
class LocalActuationPeerVerifier : public PeerVerifier {
 public:
  LocalActuationPeerVerifier();
  LocalActuationPeerVerifier(const LocalActuationPeerVerifier&) = delete;
  LocalActuationPeerVerifier& operator=(const LocalActuationPeerVerifier&) =
      delete;
  ~LocalActuationPeerVerifier() override;

  // PeerVerifier:
  bool Verify(audit_token_t audit_token,
              base::span<const uint8_t> info_plist) const override;

 private:
  // All senders are rejected if `std::nullopt`.
  std::optional<base::mac::ProcessRequirement> requirement_;
};

}  // namespace browser_actuator

#endif  // CHROME_BROWSER_BROWSER_ACTUATOR_LOCAL_ACTUATION_LOCAL_ACTUATION_PEER_VERIFIER_H_
