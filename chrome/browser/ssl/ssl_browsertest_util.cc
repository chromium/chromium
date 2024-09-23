// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_browsertest_util.h"

#include "base/feature_list.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/common/chrome_features.h"
#include "components/security_state/content/security_state_tab_helper.h"
#include "components/security_state/core/security_state.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/page_type.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "net/base/features.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/ev_root_ca_metadata.h"
#include "net/net_buildflags.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ssl_test_util {

namespace AuthState {

void Check(content::NavigationEntry* entry, int expected_authentication_state) {
  if (expected_authentication_state == AuthState::SHOWING_ERROR ||
      expected_authentication_state == AuthState::SHOWING_INTERSTITIAL) {
    EXPECT_EQ(content::PAGE_TYPE_ERROR, entry->GetPageType());
  } else {
    EXPECT_EQ(content::PAGE_TYPE_NORMAL, entry->GetPageType());
  }

  bool displayed_insecure_content =
      !!(entry->GetSSL().content_status &
         content::SSLStatus::DISPLAYED_INSECURE_CONTENT);
  EXPECT_EQ(
      !!(expected_authentication_state & AuthState::DISPLAYED_INSECURE_CONTENT),
      displayed_insecure_content);

  bool ran_insecure_content = !!(entry->GetSSL().content_status &
                                 content::SSLStatus::RAN_INSECURE_CONTENT);
  EXPECT_EQ(!!(expected_authentication_state & AuthState::RAN_INSECURE_CONTENT),
            ran_insecure_content);

  bool displayed_form_with_insecure_action =
      !!(entry->GetSSL().content_status &
         content::SSLStatus::DISPLAYED_FORM_WITH_INSECURE_ACTION);
  EXPECT_EQ(!!(expected_authentication_state &
               AuthState::DISPLAYED_FORM_WITH_INSECURE_ACTION),
            displayed_form_with_insecure_action);
}

}  // namespace AuthState

namespace SecurityStyle {

void Check(content::WebContents* tab,
           security_state::SecurityLevel expected_security_level) {
  SecurityStateTabHelper* helper = SecurityStateTabHelper::FromWebContents(tab);
  EXPECT_EQ(expected_security_level, helper->GetSecurityLevel());
}

}  // namespace SecurityStyle

namespace CertError {

void Check(content::NavigationEntry* entry, net::CertStatus error) {
  if (error) {
    EXPECT_EQ(error, entry->GetSSL().cert_status & error);
    net::CertStatus extra_cert_errors =
        error ^ (entry->GetSSL().cert_status & net::CERT_STATUS_ALL_ERRORS);
    EXPECT_FALSE(extra_cert_errors)
        << "Got unexpected cert error: " << extra_cert_errors;
  } else {
    EXPECT_EQ(0U, entry->GetSSL().cert_status & net::CERT_STATUS_ALL_ERRORS);
  }
}

}  // namespace CertError

void CheckSecurityState(content::WebContents* tab,
                        net::CertStatus expected_error,
                        security_state::SecurityLevel expected_security_level,
                        int expected_authentication_state) {
  ASSERT_FALSE(tab->IsCrashed());
  // TODO(crbug.com/40688528): Check if this can be replaced with
  // GetLastCommittedEntry.
  content::NavigationEntry* entry = tab->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  CertError::Check(entry, expected_error);
  SecurityStyle::Check(tab, expected_security_level);
  AuthState::Check(entry, expected_authentication_state);
}

void CheckAuthenticatedState(content::WebContents* tab,
                             int expected_authentication_state) {
  CheckSecurityState(tab, CertError::NONE, security_state::SECURE,
                     expected_authentication_state);
}

void CheckUnauthenticatedState(content::WebContents* tab,
                               int expected_authentication_state) {
  CheckSecurityState(tab, CertError::NONE, security_state::NONE,
                     expected_authentication_state);
}

void CheckAuthenticationBrokenState(content::WebContents* tab,
                                    net::CertStatus expected_error,
                                    int expected_authentication_state) {
  CheckSecurityState(tab, expected_error, security_state::DANGEROUS,
                     expected_authentication_state);
}

SecurityStateWebContentsObserver::SecurityStateWebContentsObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

SecurityStateWebContentsObserver::~SecurityStateWebContentsObserver() {}

void SecurityStateWebContentsObserver::WaitForDidChangeVisibleSecurityState() {
  run_loop_.Run();
}

void SecurityStateWebContentsObserver::DidChangeVisibleSecurityState() {
  run_loop_.Quit();
}

bool UsingBuiltinCertVerifier() {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
  return true;
#else
  return false;
#endif
}

bool SystemSupportsHardFailRevocationChecking() {
  return UsingBuiltinCertVerifier();
}

bool SystemUsesChromiumEVMetadata() {
#if defined(PLATFORM_USES_CHROMIUM_EV_METADATA)
  return true;
#else
  return false;
#endif
}

bool SystemSupportsOCSPStapling() {
  if (UsingBuiltinCertVerifier())
    return true;
#if BUILDFLAG(IS_ANDROID)
  return false;
#else
  return true;
#endif
}

bool CertVerifierSupportsCRLSetBlocking() {
  if (UsingBuiltinCertVerifier())
    return true;
#if BUILDFLAG(IS_ANDROID)
  return false;
#else
  return true;
#endif
}

void SetHSTSForHostName(content::BrowserContext* context,
                        const std::string& hostname) {
  const base::Time expiry = base::Time::Now() + base::Days(1000);
  bool include_subdomains = false;
  mojo::ScopedAllowSyncCallForTesting allow_sync_call;
  content::StoragePartition* partition = context->GetDefaultStoragePartition();
  base::RunLoop run_loop;
  partition->GetNetworkContext()->AddHSTS(hostname, expiry, include_subdomains,
                                          run_loop.QuitClosure());
  run_loop.Run();
}

}  // namespace ssl_test_util
