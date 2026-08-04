// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_CHROME_SECURITY_STATE_UTIL_H_
#define CHROME_BROWSER_SSL_CHROME_SECURITY_STATE_UTIL_H_

#include <memory>

#include "components/security_state/core/security_state.h"

namespace content {
class WebContents;
}  // namespace content

namespace chrome_security_state {

// Returns the security state of `web_contents`' visible navigation entry,
// enriched with Chrome embedder information: Safe Browsing verdicts, safety
// tips, HTTPS-Only Mode upgrade state, 2-QWAC certificates and the mixed
// forms policy. Pure reads of current state; nothing is cached or stored on
// the WebContents.
std::unique_ptr<security_state::VisibleSecurityState> GetVisibleSecurityState(
    content::WebContents* web_contents);

// Returns the security level computed from GetVisibleSecurityState().
security_state::SecurityLevel GetSecurityLevel(
    content::WebContents* web_contents);

// Returns the Safe Browsing malicious content status for `web_contents`'
// visible navigation entry, or MALICIOUS_CONTENT_STATUS_NONE when Safe
// Browsing is unavailable.
security_state::MaliciousContentStatus GetMaliciousContentStatus(
    content::WebContents* web_contents);

// Overrides the malicious content status returned by
// GetMaliciousContentStatus() (and therefore the one written into
// GetVisibleSecurityState()) for the lifetime of this object. Allows tests
// to drive DANGEROUS security levels that a test navigation cannot produce.
class ScopedMaliciousContentStatusForTesting {
 public:
  explicit ScopedMaliciousContentStatusForTesting(
      security_state::MaliciousContentStatus status);
  ScopedMaliciousContentStatusForTesting(
      const ScopedMaliciousContentStatusForTesting&) = delete;
  ScopedMaliciousContentStatusForTesting& operator=(
      const ScopedMaliciousContentStatusForTesting&) = delete;
  ~ScopedMaliciousContentStatusForTesting();
};

}  // namespace chrome_security_state

#endif  // CHROME_BROWSER_SSL_CHROME_SECURITY_STATE_UTIL_H_
