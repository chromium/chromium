// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/content/stateful_ssl_host_state_delegate.h"

#include <stdint.h>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/simple_test_clock.h"
#include "build/build_config.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/stateful_ssl_host_state_delegate_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/ssl_host_state_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/browsing_data_remover_test_util.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/common/switches.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kOkCertFile[] = "ok_cert.pem";

const char kWWWGoogleHost[] = "www.google.com";
const char kGoogleHost[] = "google.com";
const char kExampleHost[] = "example.com";

const uint64_t kDeltaOneDayInSeconds = UINT64_C(86400);
const uint64_t kDeltaOneWeekInSeconds = UINT64_C(604800);
const uint64_t kDeltaFifteenDaysInSeconds = UINT64_C(1296000);

scoped_refptr<net::X509Certificate> GetOkCert() {
  return net::ImportCertFromFile(net::GetTestCertsDirectory(), kOkCertFile);
}

bool CStrStringMatcher(const char* a, const std::string& b) {
  return a == b;
}

}  // namespace

class StatefulSSLHostStateDelegateTest : public InProcessBrowserTest {};

// StatefulSSLHostStateDelegateTest tests basic unit test functionality of the
// SSLHostStateDelegate class.  For example, tests that if a certificate is
// accepted, then it is added to queryable, and if it is revoked, it is not
// queryable. Even though it is effectively a unit test, in needs to be an
// InProcessBrowserTest because the actual functionality is provided by
// StatefulSSLHostStateDelegate which is provided per-profile.
//
// QueryPolicy unit tests the expected behavior of calling QueryPolicy on the
// SSLHostStateDelegate class after various SSL cert decisions have been made.
IN_PROC_BROWSER_TEST_F(StatefulSSLHostStateDelegateTest, QueryPolicy) {
  scoped_refptr<net::X509Certificate> cert = GetOkCert();
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  content::SSLHostStateDelegate* state = profile->GetSSLHostStateDelegate();
  auto* storage_partition = tab->GetPrimaryMainFrame()->GetStoragePartition();

  // Verifying that all three of the certs we will be looking at are denied
  // before any action has been taken.
  EXPECT_EQ(content::SSLHostStateDelegate::DENIED,
            state->QueryPolicy(kWWWGoogleHost, *cert,
                               net::ERR_CERT_DATE_INVALID, storage_partition));
  EXPECT_EQ(content::SSLHostStateDelegate::DENIED,
            state->QueryPolicy(kGoogleHost, *cert, net::ERR_CERT_DATE_INVALID,
                               storage_partition));
  EXPECT_EQ(content::SSLHostStateDelegate::DENIED,
            state->QueryPolicy(kExampleHost, *cert, net::ERR_CERT_DATE_INVALID,
                               storage_partition));

  // Simulate a user decision to allow an invalid certificate exception for
  // kWWWGoogleHost.
  state->AllowCert(kWWWGoogleHost, *cert, net::ERR_CERT_DATE_INVALID,
                   storage_partition);

  // Verify that only kWWWGoogleHost is allowed and that the other two certs
  // being tested still are denied.
  EXPECT_EQ(content::SSLHostStateDelegate::ALLOWED,
            state->QueryPolicy(kWWWGoogleHost, *cert,
                               net::ERR_CERT_DATE_INVALID, storage_partition));
  EXPECT_EQ(content::SSLHostStateDelegate::DENIED,
            state->QueryPolicy(kGoogleHost, *cert, net::ERR_CERT_DATE_INVALID,
                               storage_partition));
  EXPECT_EQ(content::SSLHostStateDelegate::DENIED,
            state->QueryPolicy(kExampleHost, *cert, net::ERR_CERT_DATE_INVALID,
                               storage_partition));

  // Simulate a user decision to allow an invalid certificate exception for
  // kExampleHost.
  state->AllowCert(kExampleHost, *cert, net::ERR_CERT_DATE_INVALID,
                   storage_partition);

  // Verify that both kWWWGoogleHost and kExampleHost have allow exceptions
  // while kGoogleHost still is denied.
  EXPECT_EQ(content::SSLHostStateDelegate::ALLOWED,
            state->QueryPolicy(kWWWGoogleHost, *cert,
                               net::ERR_CERT_DATE_INVALID, storage_partition));
  EXPECT_EQ(content::SSLHostStateDelegate::DENIED,
            state->QueryPolicy(kGoogleHost, *cert, net::ERR_CERT_DATE_INVALID,
                               storage_partition));
  EXPECT_EQ(content::SSLHostStateDelegate::ALLOWED,
            state->QueryPolicy(kExampleHost, *cert, net::ERR_CERT_DATE_INVALID,
                               storage_partition));
}

// Tests the expected behavior of calling HasAllowExceptionForAnyHost on the
// SSLHostStateDelegate class after setting website settings for
// different ContentSettingsType.
IN_PROC_BROWSER_TEST_F(StatefulSSLHostStateDelegateTest,
                       HasAllowExceptionForAnyHost) {
  scoped_refptr<net::X509Certificate> cert = GetOkCert();
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  content::SSLHostStateDelegate* state = profile->GetSSLHostStateDelegate();
  auto* storage_partition = tab->GetPrimaryMainFrame()->GetStoragePartition();
  auto* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  GURL url = GURL("https://example1.com/");

  EXPECT_EQ(false, state->HasAllowExceptionForAnyHost(storage_partition));

  host_content_settings_map->SetContentSettingDefaultScope(
      url, url, ContentSettingsType::COOKIES, CONTENT_SETTING_DEFAULT);
  EXPECT_EQ(false, state->HasAllowExceptionForAnyHost(storage_partition));

  // Simulate a user decision to allow an invalid certificate exception for
  // kWWWGoogleHost.
  state->AllowCert(kWWWGoogleHost, *cert, net::ERR_CERT_DATE_INVALID,
                   storage_partition);
  EXPECT_EQ(true, state->HasAllowExceptionForAnyHost(storage_partition));
}

// Tests the expected behavior of calling IsHttpAllowedForHost on the
// SSLHostStateDelegate class after various HTTP decisions have been made.
IN_PROC_BROWSER_TEST_F(StatefulSSLHostStateDelegateTest, HttpAllowlisting) {
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();
  auto* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  auto* state = profile->GetSSLHostStateDelegate();
  auto* storage_partition = tab->GetPrimaryMainFrame()->GetStoragePartition();

  // Verify that all three of the hosts are not allowlisted before any action
  // has been taken.
  EXPECT_FALSE(state->IsHttpAllowedForHost(kWWWGoogleHost, storage_partition));
  EXPECT_FALSE(state->IsHttpAllowedForHost(kGoogleHost, storage_partition));
  EXPECT_FALSE(state->IsHttpAllowedForHost(kExampleHost, storage_partition));

  // Simulate a user decision to allow HTTP for kWWWGoogleHost.
  state->AllowHttpForHost(kWWWGoogleHost, storage_partition);

  // Verify that only kWWWGoogleHost is allowed and that the other two hosts
  // being tested are not.
  EXPECT_TRUE(state->IsHttpAllowedForHost(kWWWGoogleHost, storage_partition));
  EXPECT_FALSE(state->IsHttpAllowedForHost(kGoogleHost, storage_partition));
  EXPECT_FALSE(state->IsHttpAllowedForHost(kExampleHost, storage_partition));

  // Simulate a user decision to allow HTTP for kExampleHost.
  state->AllowHttpForHost(kExampleHost, storage_partition);

  // Verify that both kWWWGoogleHost and kExampleHost have allow exceptions
  // while kGoogleHost still does not.
  EXPECT_TRUE(state->IsHttpAllowedForHost(kWWWGoogleHost, storage_partition));
  EXPECT_FALSE(state->IsHttpAllowedForHost(kGoogleHost, storage_partition));
  EXPECT_TRUE(state->IsHttpAllowedForHost(kExampleHost, storage_partition));
}

// HasPolicyAndRevoke unit tests the expected behavior of calling
// HasAllowException before and after calling RevokeUserAllowExceptions on the
// SSLHostStateDelegate class.
IN_PROC_BROWSER_TEST_F(StatefulSSLHostStateDelegateTest, HasPolicyAndRevoke) {
  scoped_refptr<net::X509Certificate> cert = GetOkCert();
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  StatefulSSLHostStateDelegate* state =
      StatefulSSLHostStateDelegateFactory::GetForProfile(profile);
  auto* storage_partition = tab->GetPrimaryMainFrame()->GetStoragePartition();

  // Simulate a user decision to allow an invalid certificate exception for
  // kWWWGoogleHost and for kExampleHost.
  state->AllowCert(kWWWGoogleHost, *cert, net::ERR_CERT_DATE_INVALID,
                   storage_partition);
  state->AllowCert(kExampleHost, *cert, net::ERR_CERT_DATE_INVALID,
                   storage_partition);

  // Verify that HasAllowException correctly acknowledges that a user decision
  // has been made about kWWWGoogleHost. Then verify that HasAllowException
  // correctly identifies that the decision has been revoked.
  EXPECT_TRUE(state->HasAllowException(kWWWGoogleHost, storage_partition));
  state->RevokeUserAllowExceptions(kWWWGoogleHost);
  EXPECT_FALSE(state->HasAllowException(kWWWGoogleHost, storage_partition));
  EXPECT_EQ(content::SSLHostStateDelegate::DENIED,
            state->QueryPolicy(kWWWGoogleHost, *cert,
                               net::ERR_CERT_DATE_INVALID, storage_partition));

  // Verify that the revocation of the kWWWGoogleHost decision does not affect
  // the Allow for kExampleHost.
  EXPECT_TRUE(state->HasAllowException(kExampleHost, storage_partition));

  // Verify the revocation of the kWWWGoogleHost decision does not affect the
  // non-decision for kGoogleHost. Then verify that a revocation of a URL with
  // no decision has no effect.
  EXPECT_FALSE(state->HasAllowException(kGoogleHost, storage_partition));
  state->RevokeUserAllowExceptions(kGoogleHost);
  EXPECT_FALSE(state->HasAllowException(kGoogleHost, storage_partition));

  // Simulate a user decision to allow HTTP for kExampleHost.
  EXPECT_FALSE(state->IsHttpAllowedForHost(kExampleHost, storage_partition));
  state->AllowHttpForHost(kExampleHost, storage_partition);

  // Verify that revoking for kExampleHost clears both the cert decision and the
  // HTTP decision.
  EXPECT_TRUE(state->HasAllowException(kExampleHost, storage_partition));
  EXPECT_TRUE(state->IsHttpAllowedForHost(kExampleHost, storage_partition));
  state->RevokeUserAllowExceptions(kExampleHost);
  EXPECT_FALSE(state->HasAllowException(kExampleHost, storage_partition));
}

// Clear unit tests the expected behavior of calling Clear to forget all cert
// decision state and HTTP allowlist state on the SSLHostStateDelegate class.
IN_PROC_BROWSER_TEST_F(StatefulSSLHostStateDelegateTest, Clear) {
  scoped_refptr<net::X509Certificate> cert = GetOkCert();
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  StatefulSSLHostStateDelegate* state =
      StatefulSSLHostStateDelegateFactory::GetForProfile(profile);
  auto* storage_partition = tab->GetPrimaryMainFrame()->GetStoragePartition();

  // Simulate a user decision to allow an invalid certificate exception for
  // kWWWGoogleHost and for kExampleHost.
  state->AllowCert(kWWWGoogleHost, *cert, net::ERR_CERT_DATE_INVALID,
                   storage_partition);
  state->AllowCert(kExampleHost, *cert, net::ERR_CERT_DATE_INVALID,
                   storage_partition);

  EXPECT_TRUE(state->HasAllowException(kWWWGoogleHost, storage_partition));
  EXPECT_TRUE(state->HasAllowException(kExampleHost, storage_partition));

  // Clear data for kWWWGoogleHost. kExampleHost will not be modified.
  state->Clear(base::BindRepeating(&CStrStringMatcher,
                                   base::Unretained(kWWWGoogleHost)));

  EXPECT_FALSE(state->HasAllowException(kWWWGoogleHost, storage_partition));
  EXPECT_TRUE(state->HasAllowException(kExampleHost, storage_partition));

  // Do a full clear, then make sure that both kWWWGoogleHost and kExampleHost,
  // which had a decision made, and kGoogleHost, which was untouched, are now
  // in a denied state.
  state->Clear(base::RepeatingCallback<bool(const std::string&)>());
  EXPECT_FALSE(state->HasAllowException(kWWWGoogleHost, storage_partition));
  EXPECT_EQ(content::SSLHostStateDelegate::DENIED,
            state->QueryPolicy(kWWWGoogleHost, *cert,
                               net::ERR_CERT_DATE_INVALID, storage_partition));
  EXPECT_FALSE(state->HasAllowException(kExampleHost, storage_partition));
  EXPECT_EQ(content::SSLHostStateDelegate::DENIED,
            state->QueryPolicy(kExampleHost, *cert, net::ERR_CERT_DATE_INVALID,
                               storage_partition));
  EXPECT_FALSE(state->HasAllowException(kGoogleHost, storage_partition));
  EXPECT_EQ(content::SSLHostStateDelegate::DENIED,
            state->QueryPolicy(kGoogleHost, *cert, net::ERR_CERT_DATE_INVALID,
                               storage_partition));

  // Simulate a user decision to allow HTTP for kWWWGoogleHost and for
  // kExampleHost.
  state->AllowHttpForHost(kWWWGoogleHost, storage_partition);
  state->AllowHttpForHost(kExampleHost, storage_partition);

  EXPECT_TRUE(state->HasAllowException(kWWWGoogleHost, storage_partition));
  EXPECT_TRUE(state->HasAllowException(kExampleHost, storage_partition));

  // Clear data for kWWWGoogleHost. kExampleHost will not be modified.
  state->Clear(base::BindRepeating(&CStrStringMatcher,
                                   base::Unretained(kWWWGoogleHost)));

  EXPECT_FALSE(state->HasAllowException(kWWWGoogleHost, storage_partition));
  EXPECT_TRUE(state->HasAllowException(kExampleHost, storage_partition));

  // Do a full clear, then make sure that both kWWWGoogleHost and kExampleHost,
  // which had a decision made, and kGoogleHost, which was untouched, do not
  // have HTTP allowlist entries.
  state->Clear(base::RepeatingCallback<bool(const std::string&)>());
  EXPECT_FALSE(state->HasAllowException(kWWWGoogleHost, storage_partition));
  EXPECT_FALSE(state->IsHttpAllowedForHost(kWWWGoogleHost, storage_partition));
  EXPECT_FALSE(state->HasAllowException(kExampleHost, storage_partition));
  EXPECT_FALSE(state->IsHttpAllowedForHost(kExampleHost, storage_partition));
  EXPECT_FALSE(state->HasAllowException(kGoogleHost, storage_partition));
  EXPECT_FALSE(state->IsHttpAllowedForHost(kGoogleHost, storage_partition));
}

// DidHostRunInsecureContent unit tests the expected behavior of calling
// DidHostRunInsecureContent as well as HostRanInsecureContent to check if
// insecure content has been run and to mark it as such.
IN_PROC_BROWSER_TEST_F(StatefulSSLHostStateDelegateTest,
                       DidHostRunInsecureContent) {
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  content::SSLHostStateDelegate* state = profile->GetSSLHostStateDelegate();

  EXPECT_FALSE(state->DidHostRunInsecureContent(
      "www.google.com", 42, content::SSLHostStateDelegate::MIXED_CONTENT));
  EXPECT_FALSE(state->DidHostRunInsecureContent(
      "www.google.com", 191, content::SSLHostStateDelegate::MIXED_CONTENT));
  EXPECT_FALSE(state->DidHostRunInsecureContent(
      "example.com", 42, content::SSLHostStateDelegate::MIXED_CONTENT));
  EXPECT_FALSE(state->DidHostRunInsecureContent(
      "www.google.com", 42,
      content::SSLHostStateDelegate::CERT_ERRORS_CONTENT));
  EXPECT_FALSE(state->DidHostRunInsecureContent(
      "www.google.com", 191,
      content::SSLHostStateDelegate::CERT_ERRORS_CONTENT));
  EXPECT_FALSE(state->DidHostRunInsecureContent(
      "example.com", 42, content::SSLHostStateDelegate::CERT_ERRORS_CONTENT));

  // Mark a site as MIXED_CONTENT and check that only that host/child id
  // is affected, and only for MIXED_CONTENT (not for
  // CERT_ERRORS_CONTENT);
  state->HostRanInsecureContent("www.google.com", 42,
                                content::SSLHostStateDelegate::MIXED_CONTENT);

  EXPECT_TRUE(state->DidHostRunInsecureContent(
      "www.google.com", 42, content::SSLHostStateDelegate::MIXED_CONTENT));
  EXPECT_FALSE(state->DidHostRunInsecureContent(
      "www.google.com", 42,
      content::SSLHostStateDelegate::CERT_ERRORS_CONTENT));
  EXPECT_FALSE(state->DidHostRunInsecureContent(
      "www.google.com", 191, content::SSLHostStateDelegate::MIXED_CONTENT));
  EXPECT_FALSE(state->DidHostRunInsecureContent(
      "example.com", 42, content::SSLHostStateDelegate::MIXED_CONTENT));

  // Mark another site as MIXED_CONTENT, and check that that host/child
  // id is affected (for MIXED_CONTENT only), and that the previously
  // host/child id is still marked as MIXED_CONTENT.
  state->HostRanInsecureContent("example.com", 42,
                                content::SSLHostStateDelegate::MIXED_CONTENT);

  EXPECT_TRUE(state->DidHostRunInsecureContent(
      "www.google.com", 42, content::SSLHostStateDelegate::MIXED_CONTENT));
  EXPECT_FALSE(state->DidHostRunInsecureContent(
      "www.google.com", 191, content::SSLHostStateDelegate::MIXED_CONTENT));
  EXPECT_TRUE(state->DidHostRunInsecureContent(
      "example.com", 42, content::SSLHostStateDelegate::MIXED_CONTENT));
  EXPECT_FALSE(state->DidHostRunInsecureContent(
      "example.com", 42, content::SSLHostStateDelegate::CERT_ERRORS_CONTENT));

  // Mark a MIXED_CONTENT host/child id as CERT_ERRORS_CONTENT also.
  state->HostRanInsecureContent(
      "example.com", 42, content::SSLHostStateDelegate::CERT_ERRORS_CONTENT);

  EXPECT_FALSE(state->DidHostRunInsecureContent(
      "www.google.com", 191, content::SSLHostStateDelegate::MIXED_CONTENT));
  EXPECT_TRUE(state->DidHostRunInsecureContent(
      "example.com", 42, content::SSLHostStateDelegate::MIXED_CONTENT));
  EXPECT_TRUE(state->DidHostRunInsecureContent(
      "example.com", 42, content::SSLHostStateDelegate::CERT_ERRORS_CONTENT));

  // Mark a non-MIXED_CONTENT host as CERT_ERRORS_CONTENT.
  state->HostRanInsecureContent(
      "www.google.com", 191,
      content::SSLHostStateDelegate::CERT_ERRORS_CONTENT);

  EXPECT_TRUE(state->DidHostRunInsecureContent(
      "www.google.com", 191,
      content::SSLHostStateDelegate::CERT_ERRORS_CONTENT));
  EXPECT_FALSE(state->DidHostRunInsecureContent(
      "www.google.com", 191, content::SSLHostStateDelegate::MIXED_CONTENT));
}

// Tests that StatefulSSLHostStateDelegate::HasSeenRecurrentErrors returns true
// after seeing an error of interest multiple times, in the default mode in
// which error occurrences are stored in-memory.
IN_PROC_BROWSER_TEST_F(StatefulSSLHostStateDelegateTest,
                       HasSeenRecurrentErrors) {
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  content::SSLHostStateDelegate* state = profile->GetSSLHostStateDelegate();
  StatefulSSLHostStateDelegate* chrome_state =
      static_cast<StatefulSSLHostStateDelegate*>(state);
  chrome_state->SetRecurrentInterstitialThresholdForTesting(2);
  chrome_state->SetRecurrentInterstitialModeForTesting(
      StatefulSSLHostStateDelegate::RecurrentInterstitialMode::PREF);

  chrome_state->DidDisplayErrorPage(net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED);
  EXPECT_FALSE(chrome_state->HasSeenRecurrentErrors(
      net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED));
  chrome_state->DidDisplayErrorPage(net::ERR_CERT_SYMANTEC_LEGACY);
  EXPECT_FALSE(chrome_state->HasSeenRecurrentErrors(
      net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED));
  chrome_state->DidDisplayErrorPage(net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED);
  EXPECT_TRUE(chrome_state->HasSeenRecurrentErrors(
      net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED));
}

// Tests that StatefulSSLHostStateDelegate::HasSeenRecurrentErrors returns true
// after seeing an error of interest multiple times in pref mode (where the
// count of each error is persisted across browsing sessions).
IN_PROC_BROWSER_TEST_F(StatefulSSLHostStateDelegateTest,
                       HasSeenRecurrentErrorsPref) {
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  content::SSLHostStateDelegate* state = profile->GetSSLHostStateDelegate();
  StatefulSSLHostStateDelegate* chrome_state =
      static_cast<StatefulSSLHostStateDelegate*>(state);
  chrome_state->SetRecurrentInterstitialThresholdForTesting(2);
  chrome_state->SetRecurrentInterstitialModeForTesting(
      StatefulSSLHostStateDelegate::RecurrentInterstitialMode::PREF);

  chrome_state->DidDisplayErrorPage(net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED);
  EXPECT_FALSE(chrome_state->HasSeenRecurrentErrors(
      net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED));
  chrome_state->DidDisplayErrorPage(net::ERR_CERT_SYMANTEC_LEGACY);
  EXPECT_FALSE(
      chrome_state->HasSeenRecurrentErrors(net::ERR_CERT_SYMANTEC_LEGACY));
  chrome_state->DidDisplayErrorPage(net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED);
  EXPECT_TRUE(chrome_state->HasSeenRecurrentErrors(
      net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED));
  chrome_state->DidDisplayErrorPage(net::ERR_CERT_SYMANTEC_LEGACY);
  EXPECT_TRUE(
      chrome_state->HasSeenRecurrentErrors(net::ERR_CERT_SYMANTEC_LEGACY));

  // Create a new StatefulSSLHostStateDelegate to check that the state has been
  // saved to the pref and that the new StatefulSSLHostStateDelegate reads it.
  StatefulSSLHostStateDelegate new_state(
      profile, profile->GetPrefs(),
      HostContentSettingsMapFactory::GetForProfile(profile));
  new_state.SetRecurrentInterstitialThresholdForTesting(2);
  new_state.SetRecurrentInterstitialModeForTesting(
      StatefulSSLHostStateDelegate::RecurrentInterstitialMode::PREF);

  EXPECT_TRUE(new_state.HasSeenRecurrentErrors(
      net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED));
  EXPECT_TRUE(new_state.HasSeenRecurrentErrors(net::ERR_CERT_SYMANTEC_LEGACY));

  // Also test the logic for when the number of displayed errors exceeds the
  // threshold.
  new_state.DidDisplayErrorPage(net::ERR_CERT_SYMANTEC_LEGACY);
  EXPECT_TRUE(new_state.HasSeenRecurrentErrors(net::ERR_CERT_SYMANTEC_LEGACY));
}

// Tests that StatefulSSLHostStateDelegate::HasSeenRecurrentErrors handles
// clocks going backwards in pref mode.
IN_PROC_BROWSER_TEST_F(StatefulSSLHostStateDelegateTest,
                       HasSeenRecurrentErrorsPrefClockGoesBackwards) {
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  content::SSLHostStateDelegate* state = profile->GetSSLHostStateDelegate();
  StatefulSSLHostStateDelegate* chrome_state =
      static_cast<StatefulSSLHostStateDelegate*>(state);
  chrome_state->SetRecurrentInterstitialThresholdForTesting(2);
  chrome_state->SetRecurrentInterstitialModeForTesting(
      StatefulSSLHostStateDelegate::RecurrentInterstitialMode::PREF);

  base::SimpleTestClock* clock = new base::SimpleTestClock();
  clock->SetNow(base::Time::Now());
  chrome_state->SetClockForTesting(
      std::unique_ptr<base::SimpleTestClock>(clock));

  chrome_state->DidDisplayErrorPage(net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED);
  EXPECT_FALSE(chrome_state->HasSeenRecurrentErrors(
      net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED));

  // Move the clock backwards and test that the recurrent error state is reset.
  clock->Advance(-base::Seconds(10));
  chrome_state->DidDisplayErrorPage(net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED);
  EXPECT_FALSE(chrome_state->HasSeenRecurrentErrors(
      net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED));

  // If the clock continues to move forwards, a subsequent error page should
  // trigger the recurrent error message.
  clock->Advance(base::Seconds(10));
  chrome_state->DidDisplayErrorPage(net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED);
  EXPECT_TRUE(chrome_state->HasSeenRecurrentErrors(
      net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED));
}

// Tests that StatefulSSLHostStateDelegate::HasSeenRecurrentErrors in pref mode
// ignores errors that occurred too far in the past. Note that this test uses a
// threshold of 3 errors, unlike previous tests which use a threshold of 2.
IN_PROC_BROWSER_TEST_F(StatefulSSLHostStateDelegateTest,
                       HasSeenRecurrentErrorsPrefErrorsInPast) {
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  content::SSLHostStateDelegate* state = profile->GetSSLHostStateDelegate();
  StatefulSSLHostStateDelegate* chrome_state =
      static_cast<StatefulSSLHostStateDelegate*>(state);
  chrome_state->SetRecurrentInterstitialResetTimeForTesting(10);
  chrome_state->SetRecurrentInterstitialModeForTesting(
      StatefulSSLHostStateDelegate::RecurrentInterstitialMode::PREF);

  base::SimpleTestClock* clock = new base::SimpleTestClock();
  clock->SetNow(base::Time::Now());
  chrome_state->SetClockForTesting(
      std::unique_ptr<base::SimpleTestClock>(clock));

  chrome_state->DidDisplayErrorPage(net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED);
  EXPECT_FALSE(chrome_state->HasSeenRecurrentErrors(
      net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED));

  // Subsequent errors more than 10 seconds later shouldn't trigger the
  // recurrent error message.
  clock->Advance(base::Seconds(12));
  chrome_state->DidDisplayErrorPage(net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED);
  EXPECT_FALSE(chrome_state->HasSeenRecurrentErrors(
      net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED));
  clock->Advance(base::Seconds(3));
  chrome_state->DidDisplayErrorPage(net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED);
  EXPECT_FALSE(chrome_state->HasSeenRecurrentErrors(
      net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED));

  // But a third subsequent error within 10 seconds should.
  clock->Advance(base::Seconds(3));
  chrome_state->DidDisplayErrorPage(net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED);
  EXPECT_TRUE(chrome_state->HasSeenRecurrentErrors(
      net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED));
}

// Tests the basic behavior of cert memory in incognito.
class IncognitoSSLHostStateDelegateTest
    : public StatefulSSLHostStateDelegateTest {};

IN_PROC_BROWSER_TEST_F(IncognitoSSLHostStateDelegateTest, PRE_AfterRestart) {
  scoped_refptr<net::X509Certificate> cert = GetOkCert();
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  content::SSLHostStateDelegate* state = profile->GetSSLHostStateDelegate();

  // Add a cert exception to the profile and then verify that it still exists
  // in the incognito profile.
  state->AllowCert(kWWWGoogleHost, *cert, net::ERR_CERT_DATE_INVALID,
                   tab->GetPrimaryMainFrame()->GetStoragePartition());

  auto* incognito_browser = CreateIncognitoBrowser(profile);
  auto* incognito_tab =
      incognito_browser->tab_strip_model()->GetActiveWebContents();

  Profile* incognito = profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  content::SSLHostStateDelegate* incognito_state =
      incognito->GetSSLHostStateDelegate();
  EXPECT_EQ(content::SSLHostStateDelegate::ALLOWED,
            incognito_state->QueryPolicy(
                kWWWGoogleHost, *cert, net::ERR_CERT_DATE_INVALID,
                incognito_tab->GetPrimaryMainFrame()->GetStoragePartition()));

  // Add a cert exception to the incognito profile. It will be checked after
  // restart that this exception does not exist. Note the different cert URL and
  // error than above thus mapping to a second exception. Also validate that it
  // was not added as an exception to the regular profile.
  incognito_state->AllowCert(
      kGoogleHost, *cert, net::ERR_CERT_COMMON_NAME_INVALID,
      incognito_tab->GetPrimaryMainFrame()->GetStoragePartition());

  EXPECT_EQ(
      content::SSLHostStateDelegate::DENIED,
      state->QueryPolicy(kGoogleHost, *cert, net::ERR_CERT_COMMON_NAME_INVALID,
                         tab->GetPrimaryMainFrame()->GetStoragePartition()));
}

// AfterRestart ensures that any cert decisions made in an incognito profile are
// forgetten after a session restart even if the field trial group parameter
// specifies to remember cert decisions after restart.
IN_PROC_BROWSER_TEST_F(IncognitoSSLHostStateDelegateTest, AfterRestart) {
  scoped_refptr<net::X509Certificate> cert = GetOkCert();
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  content::SSLHostStateDelegate* state = profile->GetSSLHostStateDelegate();

  // Verify that the exception added before restart to the regular
  // (non-incognito) profile still exists and was not cleared after the
  // incognito session ended.
  EXPECT_EQ(
      content::SSLHostStateDelegate::ALLOWED,
      state->QueryPolicy(kWWWGoogleHost, *cert, net::ERR_CERT_DATE_INVALID,
                         tab->GetPrimaryMainFrame()->GetStoragePartition()));

  auto* incognito_browser = CreateIncognitoBrowser(profile);
  auto* incognito_tab =
      incognito_browser->tab_strip_model()->GetActiveWebContents();

  Profile* incognito = profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  content::SSLHostStateDelegate* incognito_state =
      incognito->GetSSLHostStateDelegate();

  // Verify that the exception added before restart to the incognito profile was
  // cleared when the incognito session ended.
  EXPECT_EQ(content::SSLHostStateDelegate::DENIED,
            incognito_state->QueryPolicy(
                kGoogleHost, *cert, net::ERR_CERT_COMMON_NAME_INVALID,
                incognito_tab->GetPrimaryMainFrame()->GetStoragePartition()));
}

// TODO(crbug.com/40787070): Disabled for brokenness.
IN_PROC_BROWSER_TEST_F(IncognitoSSLHostStateDelegateTest,
                       DISABLED_PRE_AfterRestartHttp) {
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();
  auto* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  auto* state = profile->GetSSLHostStateDelegate();

  // Add an HTTP exception to the profile and then verify that it still exists
  // in the incognito profile.
  state->AllowHttpForHost(kWWWGoogleHost,
                          tab->GetPrimaryMainFrame()->GetStoragePartition());

  auto* incognito_browser = CreateIncognitoBrowser(profile);
  auto* incognito_tab =
      incognito_browser->tab_strip_model()->GetActiveWebContents();

  auto* incognito = profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  auto* incognito_state = incognito->GetSSLHostStateDelegate();
  EXPECT_TRUE(incognito_state->IsHttpAllowedForHost(
      kExampleHost,
      incognito_tab->GetPrimaryMainFrame()->GetStoragePartition()));

  // Add an HTTP exception to the incognito profile. It will be checked after
  // restart that this exception does not exist. Note the different host than
  // above thus mapping to a second exception. Also validate that it was not
  // added as an exception to the regular profile.
  incognito_state->AllowHttpForHost(
      kGoogleHost, incognito_tab->GetPrimaryMainFrame()->GetStoragePartition());
  EXPECT_FALSE(state->IsHttpAllowedForHost(
      kGoogleHost, tab->GetPrimaryMainFrame()->GetStoragePartition()));
}

// AfterRestartHttp ensures that any HTTP decisions made in an incognito profile
// are forgetten after a session restart.
// TODO(crbug.com/40787070): Disabled for brokenness.
IN_PROC_BROWSER_TEST_F(IncognitoSSLHostStateDelegateTest,
                       DISABLED_AfterRestartHttp) {
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();
  auto* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  auto* state = profile->GetSSLHostStateDelegate();

  // Verify that the exception added before restart to the regular
  // (non-incognito) profile still exists and was not cleared after the
  // incognito session ended.
  EXPECT_TRUE(state->IsHttpAllowedForHost(
      kWWWGoogleHost, tab->GetPrimaryMainFrame()->GetStoragePartition()));

  auto* incognito_browser = CreateIncognitoBrowser(profile);
  auto* incognito_tab =
      incognito_browser->tab_strip_model()->GetActiveWebContents();

  auto* incognito = profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  auto* incognito_state = incognito->GetSSLHostStateDelegate();

  // Verify that the exception added before restart to the incognito profile was
  // cleared when the incognito session ended.
  EXPECT_FALSE(incognito_state->IsHttpAllowedForHost(
      kGoogleHost,
      incognito_tab->GetPrimaryMainFrame()->GetStoragePartition()));
}

// Tests the default certificate memory, which is one week.
class DefaultMemorySSLHostStateDelegateTest
    : public StatefulSSLHostStateDelegateTest {};

IN_PROC_BROWSER_TEST_F(DefaultMemorySSLHostStateDelegateTest,
                       PRE_AfterRestart) {
  scoped_refptr<net::X509Certificate> cert = GetOkCert();
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  content::SSLHostStateDelegate* state = profile->GetSSLHostStateDelegate();

  state->AllowCert(kWWWGoogleHost, *cert, net::ERR_CERT_DATE_INVALID,
                   tab->GetPrimaryMainFrame()->GetStoragePartition());
  EXPECT_EQ(
      content::SSLHostStateDelegate::ALLOWED,
      state->QueryPolicy(kWWWGoogleHost, *cert, net::ERR_CERT_DATE_INVALID,
                         tab->GetPrimaryMainFrame()->GetStoragePartition()));
}

IN_PROC_BROWSER_TEST_F(DefaultMemorySSLHostStateDelegateTest, AfterRestart) {
  scoped_refptr<net::X509Certificate> cert = GetOkCert();
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  content::SSLHostStateDelegate* state = profile->GetSSLHostStateDelegate();
  auto* storage_partition = tab->GetPrimaryMainFrame()->GetStoragePartition();

  // chrome_state takes ownership of this clock
  base::SimpleTestClock* clock = new base::SimpleTestClock();
  StatefulSSLHostStateDelegate* chrome_state =
      static_cast<StatefulSSLHostStateDelegate*>(state);
  chrome_state->SetClockForTesting(std::unique_ptr<base::Clock>(clock));

  // Start the clock at standard system time.
  clock->SetNow(base::Time::NowFromSystemTime());

  // This should only pass if the cert was allowed before the test was restart
  // and thus has now been remembered across browser restarts.
  EXPECT_EQ(content::SSLHostStateDelegate::ALLOWED,
            state->QueryPolicy(kWWWGoogleHost, *cert,
                               net::ERR_CERT_DATE_INVALID, storage_partition));

  // Simulate the clock advancing by one day, which is less than the expiration
  // length.
  clock->Advance(base::Seconds(kDeltaOneDayInSeconds + 1));

  // The cert should still be |ALLOWED| because the default expiration length
  // has not passed yet.
  EXPECT_EQ(content::SSLHostStateDelegate::ALLOWED,
            state->QueryPolicy(kWWWGoogleHost, *cert,
                               net::ERR_CERT_DATE_INVALID, storage_partition));

  // Now simulate the clock advancing by one week, which is past the expiration
  // point.
  clock->Advance(
      base::Seconds(kDeltaOneWeekInSeconds - kDeltaOneDayInSeconds + 1));

  // The cert should now be |DENIED| because the specified delta has passed.
  EXPECT_EQ(content::SSLHostStateDelegate::DENIED,
            state->QueryPolicy(kWWWGoogleHost, *cert,
                               net::ERR_CERT_DATE_INVALID, storage_partition));
}

IN_PROC_BROWSER_TEST_F(DefaultMemorySSLHostStateDelegateTest,
                       PRE_AfterRestartHttp) {
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();
  auto* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  auto* state = profile->GetSSLHostStateDelegate();

  state->AllowHttpForHost(kWWWGoogleHost,
                          tab->GetPrimaryMainFrame()->GetStoragePartition());
  EXPECT_TRUE(state->IsHttpAllowedForHost(
      kWWWGoogleHost, tab->GetPrimaryMainFrame()->GetStoragePartition()));
}

IN_PROC_BROWSER_TEST_F(DefaultMemorySSLHostStateDelegateTest,
                       AfterRestartHttp) {
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();
  auto* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  auto* state = profile->GetSSLHostStateDelegate();
  auto* storage_partition = tab->GetPrimaryMainFrame()->GetStoragePartition();

  // `chrome_state` takes ownership of this clock.
  auto clock = std::make_unique<base::SimpleTestClock>();
  auto* clock_ptr = clock.get();
  auto* chrome_state = static_cast<StatefulSSLHostStateDelegate*>(state);
  chrome_state->SetClockForTesting(std::move(clock));

  // Start the clock at standard system time.
  clock_ptr->SetNow(base::Time::NowFromSystemTime());

  // This should only pass if HTTP was allowed before the test was restarted
  // and thus has now been remembered across browser restarts.
  EXPECT_TRUE(state->IsHttpAllowedForHost(kWWWGoogleHost, storage_partition));

  // Simulate the clock advancing by one day, which is less than the expiration
  // length.
  clock_ptr->Advance(base::Seconds(kDeltaOneDayInSeconds + 1));

  // HTTP should still be allowed because the default expiration length
  // has not passed yet.
  EXPECT_TRUE(state->IsHttpAllowedForHost(kWWWGoogleHost, storage_partition));

  // Now simulate the clock advancing by fifteen days, which is past the
  // expiration point.
  clock_ptr->Advance(
      base::Seconds(kDeltaFifteenDaysInSeconds - kDeltaOneDayInSeconds + 1));

  // HTTP should no longer be allowed because the specified delta has passed.
  EXPECT_FALSE(state->IsHttpAllowedForHost(kWWWGoogleHost, storage_partition));
}

// The same test as StatefulSSLHostStateDelegateTest.QueryPolicyExpired but now
// applied to a browser context that expires based on time, not restart. This
// unit tests to make sure that if a certificate decision has expired, the
// return value from QueryPolicy returns the correct value.
IN_PROC_BROWSER_TEST_F(DefaultMemorySSLHostStateDelegateTest,
                       QueryPolicyExpired) {
  scoped_refptr<net::X509Certificate> cert = GetOkCert();
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  content::SSLHostStateDelegate* state = profile->GetSSLHostStateDelegate();
  auto* storage_partition = tab->GetPrimaryMainFrame()->GetStoragePartition();

  // chrome_state takes ownership of this clock
  base::SimpleTestClock* clock = new base::SimpleTestClock();
  StatefulSSLHostStateDelegate* chrome_state =
      static_cast<StatefulSSLHostStateDelegate*>(state);
  chrome_state->SetClockForTesting(std::unique_ptr<base::Clock>(clock));

  // Start the clock at standard system time but do not advance at all to
  // emphasize that instant forget works.
  clock->SetNow(base::Time::NowFromSystemTime());

  // The certificate has never been seen before, so it should be UNKONWN.
  EXPECT_EQ(content::SSLHostStateDelegate::DENIED,
            state->QueryPolicy(kWWWGoogleHost, *cert,
                               net::ERR_CERT_DATE_INVALID, storage_partition));

  // After allowing the certificate, a query should say that it is allowed.
  state->AllowCert(kWWWGoogleHost, *cert, net::ERR_CERT_DATE_INVALID,
                   storage_partition);
  EXPECT_EQ(content::SSLHostStateDelegate::ALLOWED,
            state->QueryPolicy(kWWWGoogleHost, *cert,
                               net::ERR_CERT_DATE_INVALID, storage_partition));

  // Simulate the clock advancing by one week, the default expiration time.
  clock->Advance(base::Seconds(kDeltaOneWeekInSeconds + 1));

  // The decision expiration time has come, so it should indicate that the
  // certificate and error are DENIED.
  EXPECT_EQ(content::SSLHostStateDelegate::DENIED,
            state->QueryPolicy(kWWWGoogleHost, *cert,
                               net::ERR_CERT_DATE_INVALID, storage_partition));
}

// Tests that if an HTTP allowlist decision has expired, then the return value
// from IsHttpAllowedForHost returns false. Similar to the test
// DefaultMemorySSLHostStateDelegateTest.AfterRestartHttp but does not involve
// restarting the browser.
IN_PROC_BROWSER_TEST_F(DefaultMemorySSLHostStateDelegateTest,
                       HttpDecisionExpires) {
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();
  auto* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  auto* state = profile->GetSSLHostStateDelegate();
  auto* storage_partition = tab->GetPrimaryMainFrame()->GetStoragePartition();

  // `chrome_state` takes ownership of this clock.
  auto clock = std::make_unique<base::SimpleTestClock>();
  auto* clock_ptr = clock.get();
  auto* chrome_state = static_cast<StatefulSSLHostStateDelegate*>(state);
  chrome_state->SetClockForTesting(std::move(clock));

  // Start the clock at standard system time.
  clock_ptr->SetNow(base::Time::NowFromSystemTime());

  // The host has never been seen before, so it should not be allowlisted.
  EXPECT_FALSE(state->IsHttpAllowedForHost(kWWWGoogleHost, storage_partition));

  // Allowlist HTTP for the host.
  state->AllowHttpForHost(kWWWGoogleHost, storage_partition);
  EXPECT_TRUE(state->IsHttpAllowedForHost(kWWWGoogleHost, storage_partition));

  // Simulate the clock advancing by fifteen days, the default expiration time.
  clock_ptr->Advance(base::Seconds(kDeltaFifteenDaysInSeconds + 1));

  // The decision expiration time has come, so this should now return false.
  EXPECT_FALSE(state->IsHttpAllowedForHost(kWWWGoogleHost, storage_partition));
}

// Tests to make sure that if the user deletes their browser history, SSL
// exceptions will be deleted as well.
class RemoveBrowsingHistorySSLHostStateDelegateTest
    : public StatefulSSLHostStateDelegateTest {
 public:
  void RemoveAndWait(Profile* profile) {
    content::BrowsingDataRemover* remover = profile->GetBrowsingDataRemover();
    content::BrowsingDataRemoverCompletionObserver completion_observer(remover);
    remover->RemoveAndReply(
        browsing_data::CalculateBeginDeleteTime(
            browsing_data::TimePeriod::LAST_HOUR),
        browsing_data::CalculateEndDeleteTime(
            browsing_data::TimePeriod::LAST_HOUR),
        chrome_browsing_data_remover::DATA_TYPE_HISTORY,
        content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
        &completion_observer);
    completion_observer.BlockUntilCompletion();
  }
};

IN_PROC_BROWSER_TEST_F(RemoveBrowsingHistorySSLHostStateDelegateTest,
                       DeleteHistory) {
  scoped_refptr<net::X509Certificate> cert = GetOkCert();
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  content::SSLHostStateDelegate* state = profile->GetSSLHostStateDelegate();

  // Add an exception for an invalid certificate. Then remove the last hour's
  // worth of browsing history and verify that the exception has been deleted.
  state->AllowCert(kGoogleHost, *cert, net::ERR_CERT_DATE_INVALID,
                   tab->GetPrimaryMainFrame()->GetStoragePartition());
  RemoveAndWait(profile);
  EXPECT_EQ(
      content::SSLHostStateDelegate::DENIED,
      state->QueryPolicy(kGoogleHost, *cert, net::ERR_CERT_DATE_INVALID,
                         tab->GetPrimaryMainFrame()->GetStoragePartition()));
}

IN_PROC_BROWSER_TEST_F(RemoveBrowsingHistorySSLHostStateDelegateTest,
                       DeleteHistoryClearsHttpAllowlistDecision) {
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();
  auto* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  auto* state = profile->GetSSLHostStateDelegate();

  // Add an exception for HTTP on this host. Then remove the last hour's worth
  // of browsing history and verify that the exception has been deleted.
  state->AllowHttpForHost(kExampleHost,
                          tab->GetPrimaryMainFrame()->GetStoragePartition());
  RemoveAndWait(profile);
  EXPECT_FALSE(state->IsHttpAllowedForHost(
      kExampleHost, tab->GetPrimaryMainFrame()->GetStoragePartition()));
}

// Tests to make sure that localhost certificate errors are treated as
// normal errors or ignored, depending on whether the
// kAllowInsecureLocalhost flag is set.
//
// When the flag isn't set, requests to localhost with invalid
// certificates should be denied.
IN_PROC_BROWSER_TEST_F(StatefulSSLHostStateDelegateTest,
                       LocalhostErrorWithoutFlag) {
  // Serve the Google cert for localhost to generate an error.
  scoped_refptr<net::X509Certificate> cert = GetOkCert();
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  content::SSLHostStateDelegate* state = profile->GetSSLHostStateDelegate();

  EXPECT_EQ(
      content::SSLHostStateDelegate::DENIED,
      state->QueryPolicy("localhost", *cert, net::ERR_CERT_COMMON_NAME_INVALID,
                         tab->GetPrimaryMainFrame()->GetStoragePartition()));

  EXPECT_EQ(
      content::SSLHostStateDelegate::DENIED,
      state->QueryPolicy("127.0.0.1", *cert, net::ERR_CERT_COMMON_NAME_INVALID,
                         tab->GetPrimaryMainFrame()->GetStoragePartition()));
}

class StatefulSSLHostStateDelegateExtensionTest
    : public extensions::ExtensionBrowserTest {
 public:
  StatefulSSLHostStateDelegateExtensionTest() = default;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        extensions::switches::kEnableExperimentalExtensionApis);
  }

 private:
  guest_view::TestGuestViewManagerFactory factory_;
};

// Tests that certificate decisions are isolated by storage partition. In
// particular, clicking through a certificate error in a <webview> in a Chrome
// App shouldn't affect normal browsing. See https://crbug.com/639173.
IN_PROC_BROWSER_TEST_F(StatefulSSLHostStateDelegateExtensionTest,
                       StoragePartitionIsolation) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Launch a Chrome app and store a certificate exception.
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  extensions::ChromeTestExtensionLoader loader(profile);
  const extensions::Extension* app =
      LoadAndLaunchApp(test_data_dir_.AppendASCII("platform_apps")
                           .AppendASCII("web_view")
                           .AppendASCII("simple"),
                       /*uses_guest_view=*/true);
  ASSERT_TRUE(app);
  auto app_windows =
      extensions::AppWindowRegistry::Get(profile)->GetAppWindowsForApp(
          app->id());
  ASSERT_EQ(1u, app_windows.size());
  // Wait for the app's guest WebContents to load.
  guest_view::TestGuestViewManager* guest_manager =
      static_cast<guest_view::TestGuestViewManager*>(
          guest_view::TestGuestViewManager::FromBrowserContext(profile));
  auto* guest = guest_manager->WaitForSingleGuestViewCreated();
  guest_manager->WaitUntilAttached(guest);

  // Store a certificate exception for the guest.
  content::SSLHostStateDelegate* state = profile->GetSSLHostStateDelegate();
  scoped_refptr<net::X509Certificate> cert = GetOkCert();
  auto* storage_partition = guest->GetGuestMainFrame()->GetStoragePartition();
  state->AllowCert(kWWWGoogleHost, *cert, net::ERR_CERT_DATE_INVALID,
                   storage_partition);
  EXPECT_EQ(content::SSLHostStateDelegate::ALLOWED,
            state->QueryPolicy(kWWWGoogleHost, *cert,
                               net::ERR_CERT_DATE_INVALID, storage_partition));
  EXPECT_TRUE(state->HasAllowException(kWWWGoogleHost, storage_partition));

  // Test that the exception is not carried over to the guest's embedder.
  EXPECT_EQ(
      content::SSLHostStateDelegate::DENIED,
      state->QueryPolicy(kWWWGoogleHost, *cert, net::ERR_CERT_DATE_INVALID,
                         tab->GetPrimaryMainFrame()->GetStoragePartition()));
  EXPECT_FALSE(state->HasAllowException(
      kWWWGoogleHost, tab->GetPrimaryMainFrame()->GetStoragePartition()));

  // Navigate to a non-app page and test that the exception is not carried over.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));
  EXPECT_EQ(
      content::SSLHostStateDelegate::DENIED,
      state->QueryPolicy(kWWWGoogleHost, *cert, net::ERR_CERT_DATE_INVALID,
                         tab->GetPrimaryMainFrame()->GetStoragePartition()));
  EXPECT_FALSE(state->HasAllowException(
      kWWWGoogleHost, tab->GetPrimaryMainFrame()->GetStoragePartition()));
}

// Tests that HTTP warning decisions are isolated by storage partition. In
// particular, clicking through an HTTP warning in a <webview> in a Chrome
// App shouldn't affect normal browsing.
IN_PROC_BROWSER_TEST_F(StatefulSSLHostStateDelegateExtensionTest,
                       StoragePartitionIsolationHttp) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Launch a Chrome app.
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  extensions::ChromeTestExtensionLoader loader(profile);
  const extensions::Extension* app =
      LoadAndLaunchApp(test_data_dir_.AppendASCII("platform_apps")
                           .AppendASCII("web_view")
                           .AppendASCII("simple"),
                       /*uses_guest_view=*/true);
  ASSERT_TRUE(app);
  auto app_windows =
      extensions::AppWindowRegistry::Get(profile)->GetAppWindowsForApp(
          app->id());
  ASSERT_EQ(1u, app_windows.size());
  // Wait for the app's guest WebContents to load.
  guest_view::TestGuestViewManager* guest_manager =
      static_cast<guest_view::TestGuestViewManager*>(
          guest_view::TestGuestViewManager::FromBrowserContext(profile));
  auto* guest = guest_manager->WaitForSingleGuestViewCreated();
  guest_manager->WaitUntilAttached(guest);

  // Store an HTTP exception for the guest.
  content::SSLHostStateDelegate* state = profile->GetSSLHostStateDelegate();
  auto* storage_partition = guest->GetGuestMainFrame()->GetStoragePartition();
  state->AllowHttpForHost(kWWWGoogleHost, storage_partition);
  EXPECT_TRUE(state->IsHttpAllowedForHost(kWWWGoogleHost, storage_partition));
  EXPECT_TRUE(state->HasAllowException(kWWWGoogleHost, storage_partition));

  // Test that the exception is not carried over to the guest's embedder.
  EXPECT_FALSE(state->IsHttpAllowedForHost(
      kWWWGoogleHost, tab->GetPrimaryMainFrame()->GetStoragePartition()));
  EXPECT_FALSE(state->HasAllowException(
      kWWWGoogleHost, tab->GetPrimaryMainFrame()->GetStoragePartition()));

  // Navigate to a non-app page and test that the exception is not carried over.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));
  EXPECT_FALSE(state->IsHttpAllowedForHost(
      kWWWGoogleHost, tab->GetPrimaryMainFrame()->GetStoragePartition()));
  EXPECT_FALSE(state->HasAllowException(
      kWWWGoogleHost, tab->GetPrimaryMainFrame()->GetStoragePartition()));
}
