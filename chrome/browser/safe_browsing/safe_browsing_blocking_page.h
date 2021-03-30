// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Classes for managing the SafeBrowsing interstitial pages.
//
// When a user is about to visit a page the SafeBrowsing system has deemed to
// be malicious, either as malware or a phishing page, we show an interstitial
// page with some options (go back, continue) to give the user a chance to avoid
// the harmful page.
//
// The SafeBrowsingBlockingPage is created by the SafeBrowsingUIManager on the
// UI thread when we've determined that a page is malicious. The operation of
// the blocking page occurs on the UI thread, where it waits for the user to
// make a decision about what to do: either go back or continue on.
//
// The blocking page forwards the result of the user's choice back to the
// SafeBrowsingUIManager so that we can cancel the request for the new page,
// or allow it to continue.
//
// A web page may contain several resources flagged as malware/phishing.  This
// results into more than one interstitial being shown.  On the first unsafe
// resource received we show an interstitial.  Any subsequent unsafe resource
// notifications while the first interstitial is showing is queued.  If the user
// decides to proceed in the first interstitial, we display all queued unsafe
// resources in a new interstitial.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_BLOCKING_PAGE_H_
#define CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_BLOCKING_PAGE_H_

#include <map>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "components/safe_browsing/content/base_blocking_page.h"
#include "components/safe_browsing/content/base_ui_manager.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace safe_browsing {

class SafeBrowsingBlockingPageFactory;
class ThreatDetails;

class SafeBrowsingBlockingPage : public BaseBlockingPage {
 public:
  typedef security_interstitials::BaseSafeBrowsingErrorUI
      BaseSafeBrowsingErrorUI;
  // Interstitial type, used in tests.
  static const security_interstitials::SecurityInterstitialPage::TypeID
      kTypeForTesting;

  ~SafeBrowsingBlockingPage() override;

  // Creates a blocking page. |should_trigger_reporting| controls whether a
  // safe browsing extended reporting report will be created for this blocking
  // page.
  static SafeBrowsingBlockingPage* CreateBlockingPage(
      BaseUIManager* ui_manager,
      content::WebContents* web_contents,
      const GURL& main_frame_url,
      const UnsafeResource& unsafe_resource,
      bool should_trigger_reporting);

  // Makes the passed |factory| the factory used to instantiate
  // SafeBrowsingBlockingPage objects. Useful for tests.
  static void RegisterFactory(SafeBrowsingBlockingPageFactory* factory) {
    factory_ = factory;
  }

  // SecurityInterstitialPage method:
  security_interstitials::SecurityInterstitialPage::TypeID GetTypeForTesting()
      override;

 protected:
  friend class SafeBrowsingBlockingPageFactoryImpl;
  friend class SafeBrowsingBlockingPageTestBase;
  friend class SafeBrowsingBlockingPageBrowserTest;
  friend class SafeBrowsingBlockingQuietPageFactoryImpl;
  friend class SafeBrowsingBlockingQuietPageTest;
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingBlockingPageTest,
                           ProceedThenDontProceed);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingBlockingPageTest,
                           MalwareReportsDisabled);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingBlockingPageTest,
                           MalwareReportsToggling);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingBlockingPageTest,
                           ExtendedReportingNotShownOnSecurePage);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingBlockingPageTest,
                           MalwareReportsTransitionDisabled);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingBlockingPageIncognitoTest,
                           ExtendedReportingNotShownInIncognito);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingBlockingPageTest,
                           ExtendedReportingNotShownNotAllowExtendedReporting);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingBlockingPageTest,
                           ExtendedReportingNotShownForEnhancedProtection);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingBlockingPageTest, BillingPage);

  void UpdateReportingPref();  // Used for the transition from old to new pref.

  // Don't instantiate this class directly, use CreateBlockingPage instead.
  SafeBrowsingBlockingPage(
      BaseUIManager* ui_manager,
      content::WebContents* web_contents,
      const GURL& main_frame_url,
      const UnsafeResourceList& unsafe_resources,
      const BaseSafeBrowsingErrorUI::SBErrorDisplayOptions& display_options,
      bool should_trigger_reporting,
      network::SharedURLLoaderFactory* url_loader_for_testing = nullptr);

  // Called when an interstitial is closed, either due to a click through or a
  // navigation elsewhere.
  void OnInterstitialClosing() override;

  // Called when the interstitial is going away. If there is a
  // pending threat details object, we look at the user's
  // preferences, and if the option to send threat details is
  // enabled, the report is scheduled to be sent on the |ui_manager_|.
  void FinishThreatDetails(const base::TimeDelta& delay,
                           bool did_proceed,
                           int num_visits) override;

  // Whether ThreatDetails collection is in progress as part of this
  // interstitial.
  bool threat_details_in_progress_;

  // The threat source that triggers the blocking page.
  ThreatSource threat_source_;

  // The factory used to instantiate SafeBrowsingBlockingPage objects.
  // Useful for tests, so they can provide their own implementation of
  // SafeBrowsingBlockingPage.
  static SafeBrowsingBlockingPageFactory* factory_;
 private:
  static std::unique_ptr<
      security_interstitials::SecurityInterstitialControllerClient>
  CreateControllerClient(content::WebContents* web_contents,
                         const UnsafeResourceList& unsafe_resources,
                         const BaseUIManager* ui_manager);

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingBlockingPage);
};

// Factory for creating SafeBrowsingBlockingPage.  Useful for tests.
class SafeBrowsingBlockingPageFactory {
 public:
  virtual ~SafeBrowsingBlockingPageFactory() { }

  virtual SafeBrowsingBlockingPage* CreateSafeBrowsingPage(
      BaseUIManager* ui_manager,
      content::WebContents* web_contents,
      const GURL& main_frame_url,
      const SafeBrowsingBlockingPage::UnsafeResourceList& unsafe_resources,
      bool should_trigger_reporting) = 0;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_BLOCKING_PAGE_H_
