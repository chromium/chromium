// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_ACTUATOR_LOCAL_ACTUATION_PEER_VERIFIER_H_
#define CHROME_BROWSER_BROWSER_ACTUATOR_LOCAL_ACTUATION_PEER_VERIFIER_H_

#include <bsm/libbsm.h>

#include <cstdint>

#include "base/containers/span.h"

namespace browser_actuator {

// Decides whether the sender of a local-actuation Mach message may use the
// service.
class PeerVerifier {
 public:
  virtual ~PeerVerifier() = default;

  // Returns true if the sender may use the service. `audit_token` comes from
  // the trailer of the sender's message and `info_plist` from its body.
  // `info_plist` is unverified: use it only as input to a code-signature check
  // of `audit_token`. May be called on any sequence.
  virtual bool Verify(audit_token_t audit_token,
                      base::span<const uint8_t> info_plist) const = 0;
};

}  // namespace browser_actuator

#endif  // CHROME_BROWSER_BROWSER_ACTUATOR_LOCAL_ACTUATION_PEER_VERIFIER_H_
