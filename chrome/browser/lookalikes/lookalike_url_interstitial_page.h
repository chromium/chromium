// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOOKALIKES_LOOKALIKE_URL_INTERSTITIAL_PAGE_H_
#define CHROME_BROWSER_LOOKALIKES_LOOKALIKE_URL_INTERSTITIAL_PAGE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/time/time.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "content/public/browser/interstitial_page_delegate.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

class GURL;

// This class is responsible for showing/hiding the interstitial page that
// occurs when a new domain is visited and it looks suspiciously like another
// more popular domain.
class LookalikeUrlInterstitialPage
    : public security_interstitials::SecurityInterstitialPage {
 public:
  // Interstitial type, used in tests.
  static const content::InterstitialPageDelegate::TypeID kTypeForTesting;

  // Used for UKM. There is only a single MatchType per navigation.
  enum class MatchType {
    kNone = 0,
    kTopSite = 1,
    kSiteEngagement = 2,
    kEditDistance = 3,
    kEditDistanceSiteEngagement = 4,

    // Append new items to the end of the list above; do not modify or replace
    // existing values. Comment out obsolete items.
    kMaxValue = kEditDistanceSiteEngagement,
  };

  // Used for UKM. There is only a single UserAction per navigation.
  enum class UserAction {
    kInterstitialNotShown = 0,
    kClickThrough = 1,
    kAcceptSuggestion = 2,
    kCloseOrBack = 3,

    // Append new items to the end of the list above; do not modify or replace
    // existing values. Comment out obsolete items.
    kMaxValue = kCloseOrBack,
  };

  LookalikeUrlInterstitialPage(
      content::WebContents* web_contents,
      const GURL& request_url,
      ukm::SourceId source_id,
      MatchType match_type,
      std::unique_ptr<
          security_interstitials::SecurityInterstitialControllerClient>
          controller);

  ~LookalikeUrlInterstitialPage() override;

  // InterstitialPageDelegate method:
  InterstitialPageDelegate::TypeID GetTypeForTesting() override;

  // Allow easier reporting of UKM when no interstitial is shown.
  static void RecordUkmEvent(ukm::SourceId source_id,
                             MatchType match_type,
                             UserAction user_action);

 protected:
  // InterstitialPageDelegate implementation:
  void CommandReceived(const std::string& command) override;

  // SecurityInterstitialPage implementation:
  bool ShouldCreateNewNavigation() const override;
  void PopulateInterstitialStrings(
      base::DictionaryValue* load_time_data) override;
  void OnInterstitialClosing() override;
  bool ShouldDisplayURL() const override;
  int GetHTMLTemplateId() override;

 private:
  friend class LookalikeUrlNavigationThrottleBrowserTest;

  // Values added to get our shared interstitial HTML to play nice.
  void PopulateStringsForSharedHTML(base::DictionaryValue* load_time_data);

  // Record UKM iff we haven't already reported for this page.
  void ReportUkmIfNeeded(UserAction action);

  ukm::SourceId source_id_;
  MatchType match_type_;

  DISALLOW_COPY_AND_ASSIGN(LookalikeUrlInterstitialPage);
};

#endif  // CHROME_BROWSER_LOOKALIKES_LOOKALIKE_URL_INTERSTITIAL_PAGE_H_
