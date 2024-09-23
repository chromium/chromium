// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SSL_BROWSERTEST_UTIL_H_
#define CHROME_BROWSER_SSL_SSL_BROWSERTEST_UTIL_H_

#include <string>

#include "base/run_loop.h"
#include "components/security_state/core/security_state.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/cert/cert_status_flags.h"

namespace content {
class BrowserContext;
class WebContents;
}

namespace ssl_test_util {

namespace AuthState {

enum AuthStateFlags {
  NONE = 0,
  DISPLAYED_INSECURE_CONTENT = 1 << 0,
  RAN_INSECURE_CONTENT = 1 << 1,
  // TODO(crbug.com/41337702): Collapse SHOWING_INTERSTITIAL into SHOWING_ERROR
  // once committed SSL interstitials are launched. For now, we automatically
  // map SHOWING_INTERSTITIAL onto SHOWING_ERROR when committed interstitials
  // are enabled.
  SHOWING_INTERSTITIAL = 1 << 2,
  SHOWING_ERROR = 1 << 3,
  DISPLAYED_FORM_WITH_INSECURE_ACTION = 1 << 4
};

}  // namespace AuthState

namespace CertError {

enum CertErrorFlags { NONE = 0 };

}  // namespace CertError

// Checks that |tab|'s
//  - certificate status flags match |expected_error|.
//  - security level is |expected_security_level|.
//  - authentication state is |expected_authentication_state|.
//
// |expected_authentication_state| should be a AuthStateFlags.
void CheckSecurityState(content::WebContents* tab,
                        net::CertStatus expected_error,
                        security_state::SecurityLevel expected_security_level,
                        int expected_authentication_state);

// Checks that |tab|'s
//  - connection status is secure
//  - authentication state is |expected_authentication_state|
void CheckAuthenticatedState(content::WebContents* tab,
                             int expected_authentication_state);

// Checks that |tab|'s
//  - connection status is unauthenticated
//  - authentication state is |expected_authentication_state|
void CheckUnauthenticatedState(content::WebContents* tab,
                               int expected_authentication_state);

// Checks that |tab|'s
//  - certificate status flags match |expected_error|
//  - authentication state is |expected_authentication_state|
void CheckAuthenticationBrokenState(content::WebContents* tab,
                                    net::CertStatus expected_error,
                                    int expected_authentication_state);

// A WebContentsObserver that allows the user to wait for a
// DidChangeVisibleSecurityState event.
class SecurityStateWebContentsObserver : public content::WebContentsObserver {
 public:
  explicit SecurityStateWebContentsObserver(content::WebContents* web_contents);
  ~SecurityStateWebContentsObserver() override;

  void WaitForDidChangeVisibleSecurityState();

  // WebContentsObserver:
  void DidChangeVisibleSecurityState() override;

 private:
  base::RunLoop run_loop_;
};

// Returns true if Chrome will use its builtin cert verifier rather than the
// operating system's default.
bool UsingBuiltinCertVerifier();

// SystemSupportsHardFailRevocationChecking returns true iff the current
// operating system supports revocation checking and can distinguish between
// situations where a given certificate lacks any revocation information (eg:
// no CRLDistributionPoints and no OCSP Responder AuthorityInfoAccess) and when
// revocation information cannot be obtained (eg: the CRL was unreachable).
// If it does not, then tests which rely on 'hard fail' behaviour should be
// skipped.
bool SystemSupportsHardFailRevocationChecking();

// SystemUsesChromiumEVMetadata returns true iff the current operating system
// uses Chromium's EV metadata (i.e. EVRootCAMetadata). If it does not, then
// several tests are effected because our testing EV certificate won't be
// recognised as EV.
bool SystemUsesChromiumEVMetadata();

// Returns true iff OCSP stapling is supported on this operating system.
bool SystemSupportsOCSPStapling();

// Returns |true| if the default CertVerifier used by the NetworkService is
// expected to support blocking certificates that appear within a CRLSet.
bool CertVerifierSupportsCRLSetBlocking();

// Sets HSTS for |hostname|, so that all certificate errors for that host
// will be non-overridable.
void SetHSTSForHostName(content::BrowserContext* context,
                        const std::string& hostname);

}  // namespace ssl_test_util

#endif  // CHROME_BROWSER_SSL_SSL_BROWSERTEST_UTIL_H_
