// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/sct_reporting_service.h"
#include "chrome/browser/ssl/sct_reporting_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/features.h"

// Observer that tracks SCT audit reports that the SCTReportingService has seen.
class CacheNotifyObserver : public SCTReportingService::TestObserver {
 public:
  CacheNotifyObserver() = default;
  ~CacheNotifyObserver() override = default;
  CacheNotifyObserver(const CacheNotifyObserver&) = delete;
  CacheNotifyObserver& operator=(const CacheNotifyObserver&) = delete;

  // SCTReportingService::TestObserver:
  void OnSCTReportReady(const std::string& cache_key) override {
    cache_entries_seen_.push_back(cache_key);
  }

  const std::vector<std::string>& cache_entries_seen() const {
    return cache_entries_seen_;
  }

 private:
  std::vector<std::string> cache_entries_seen_;
};

class SCTReportingServiceBrowserTest : public InProcessBrowserTest {
 public:
  SCTReportingServiceBrowserTest() {
    // Set sampling rate to 1.0 to ensure deterministic behavior.
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{network::features::kSCTAuditing,
          {{network::features::kSCTAuditingSamplingRate.name, "1.0"}}}},
        {});
    SystemNetworkContextManager::SetEnableCertificateTransparencyForTesting(
        true);
  }
  ~SCTReportingServiceBrowserTest() override {
    SystemNetworkContextManager::SetEnableCertificateTransparencyForTesting(
        base::nullopt);
  }

  SCTReportingServiceBrowserTest(const SCTReportingServiceBrowserTest&) =
      delete;
  const SCTReportingServiceBrowserTest& operator=(
      const SCTReportingServiceBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());

    InProcessBrowserTest::SetUpOnMainThread();
  }

 protected:
  SCTReportingServiceFactory* factory() {
    return SCTReportingServiceFactory::GetInstance();
  }

  void SetExtendedReportingEnabled(bool enabled) {
    browser()->profile()->GetPrefs()->SetBoolean(
        prefs::kSafeBrowsingScoutReportingEnabled, enabled);
  }
  void SetSafeBrowsingEnabled(bool enabled) {
    browser()->profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled,
                                                 enabled);
  }

  SCTReportingService* service() const {
    return SCTReportingServiceFactory::GetForBrowserContext(
        browser()->profile());
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

 private:
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that reports should not be enqueued when extended reporting is not
// opted in.
IN_PROC_BROWSER_TEST_F(SCTReportingServiceBrowserTest,
                       NotOptedIn_ShouldNotEnqueueReport) {
  SetExtendedReportingEnabled(false);

  // Add an observer to track reports that get sent to the embedder.
  CacheNotifyObserver observer;
  service()->AddObserverForTesting(&observer);

  // Visit an HTTPS page.
  ui_test_utils::NavigateToURL(browser(), https_server()->GetURL("/"));

  // Check that no reports are enqueued.
  EXPECT_EQ(0u, observer.cache_entries_seen().size());

  // TODO(crbug.com/1107897): Check histograms once they are added.
}

// Tests that reports should be enqueued when extended reporting is opted in.
IN_PROC_BROWSER_TEST_F(SCTReportingServiceBrowserTest,
                       OptedIn_ShouldEnqueueReport) {
  // SetSafeBrowsingEnabled(true);
  SetExtendedReportingEnabled(true);

  // Add an observer to track reports that get sent to the embedder.
  CacheNotifyObserver observer;
  service()->AddObserverForTesting(&observer);

  // Visit an HTTPS page.
  ui_test_utils::NavigateToURL(browser(), https_server()->GetURL("/"));

  // Check that one report was enqueued.
  EXPECT_EQ(1u, observer.cache_entries_seen().size());
}

// Tests that disabling SafeBrowsing entirely should cause reports to not get
// enqueued.
IN_PROC_BROWSER_TEST_F(SCTReportingServiceBrowserTest, DisableSafebrowsing) {
  SetSafeBrowsingEnabled(false);

  CacheNotifyObserver observer;
  service()->AddObserverForTesting(&observer);

  ui_test_utils::NavigateToURL(browser(), https_server()->GetURL("/"));

  EXPECT_EQ(0u, observer.cache_entries_seen().size());
}

// Tests that we don't enqueue a report for a navigation with a cert error.
IN_PROC_BROWSER_TEST_F(SCTReportingServiceBrowserTest,
                       CertErrorDoesNotEnqueueReport) {
  SetExtendedReportingEnabled(true);

  CacheNotifyObserver observer;
  service()->AddObserverForTesting(&observer);

  // Visit a page with an invalid cert.
  ui_test_utils::NavigateToURL(browser(),
                               https_server()->GetURL("invalid.test", "/"));

  EXPECT_EQ(0u, observer.cache_entries_seen().size());
}

// Tests that reports aren't enqueued for Incognito windows.
IN_PROC_BROWSER_TEST_F(SCTReportingServiceBrowserTest,
                       IncognitoWindow_ShouldNotEnqueueReport) {
  // Enable SBER in the main profile.
  SetExtendedReportingEnabled(true);

  // Create a new Incognito window and try to enable SBER in it.
  auto* incognito = CreateIncognitoBrowser();
  incognito->profile()->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingScoutReportingEnabled, true);

  auto* service =
      SCTReportingServiceFactory::GetForBrowserContext(incognito->profile());
  CacheNotifyObserver observer;
  service->AddObserverForTesting(&observer);

  ui_test_utils::NavigateToURL(incognito, https_server()->GetURL("/"));

  EXPECT_EQ(0u, observer.cache_entries_seen().size());
}

// Tests that disabling Extended Reporting causes the cache to be cleared.
IN_PROC_BROWSER_TEST_F(SCTReportingServiceBrowserTest,
                       OptingOutClearsSCTAuditingCache) {
  // Enable SCT auditing and enqueue a report.
  SetExtendedReportingEnabled(true);

  // Add an observer to track reports that get sent to the embedder.
  CacheNotifyObserver observer;
  service()->AddObserverForTesting(&observer);

  // Visit an HTTPS page.
  ui_test_utils::NavigateToURL(browser(), https_server()->GetURL("/"));

  // Check that one report was enqueued.
  EXPECT_EQ(1u, observer.cache_entries_seen().size());

  // Disable Extended Reporting which should clear the underlying cache.
  SetExtendedReportingEnabled(false);

  // We can check that the same report gets cached again instead of being
  // deduplicated (i.e., the observer should see another cache entry
  // notification).
  SetExtendedReportingEnabled(true);
  ui_test_utils::NavigateToURL(browser(), https_server()->GetURL("/"));
  EXPECT_EQ(2u, observer.cache_entries_seen().size());
}

// TODO(crbug.com/1107975): Add test for "invalid SCTs should not get reported".
// This is blocked on https://crrev.com/c/1188845 to allow us to use the
// MockCertVerifier to mock CT results.

class SCTReportingServiceZeroSamplingRateBrowserTest
    : public SCTReportingServiceBrowserTest {
 public:
  SCTReportingServiceZeroSamplingRateBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{network::features::kSCTAuditing,
          {{network::features::kSCTAuditingSamplingRate.name, "0.0"}}}},
        {});
    SystemNetworkContextManager::SetEnableCertificateTransparencyForTesting(
        true);
  }
  ~SCTReportingServiceZeroSamplingRateBrowserTest() override {
    SystemNetworkContextManager::SetEnableCertificateTransparencyForTesting(
        base::nullopt);
  }

  SCTReportingServiceZeroSamplingRateBrowserTest(
      const SCTReportingServiceZeroSamplingRateBrowserTest&) = delete;
  const SCTReportingServiceZeroSamplingRateBrowserTest& operator=(
      const SCTReportingServiceZeroSamplingRateBrowserTest&) = delete;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that the embedder is not notified when the sampling rate is zero.
IN_PROC_BROWSER_TEST_F(SCTReportingServiceZeroSamplingRateBrowserTest,
                       EmbedderNotNotified) {
  SetExtendedReportingEnabled(true);

  // Add an observer to track reports that get sent to the embedder.
  CacheNotifyObserver observer;
  service()->AddObserverForTesting(&observer);

  // Visit an HTTPS page.
  ui_test_utils::NavigateToURL(browser(), https_server()->GetURL("/"));

  // Check that no reports are observed.
  EXPECT_EQ(0u, observer.cache_entries_seen().size());
}
