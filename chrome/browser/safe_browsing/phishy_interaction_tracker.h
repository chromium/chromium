// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_PHISHY_INTERACTION_TRACKER_H_
#define CHROME_BROWSER_SAFE_BROWSING_PHISHY_INTERACTION_TRACKER_H_

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/safe_browsing/content/browser/ui_manager.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"

namespace safe_browsing {

struct PhishyPageInteractionDetails {
  PhishyPageInteractionDetails(int occurrence_count,
                               int64_t first_timestamp,
                               int64_t last_timestamp);
  int occurrence_count;
  int64_t first_timestamp;
  int64_t last_timestamp;
};

using PhishySiteInteractionMap =
    std::map<ClientSafeBrowsingReportRequest::PhishySiteInteraction::
                 PhishySiteInteractionType,
             PhishyPageInteractionDetails>;

// PhishyInteractionTracker manages and logs interactions that users have with
// pages they've reached after bypassing the Safe Browsing interstitial.
class PhishyInteractionTracker {
 public:
  explicit PhishyInteractionTracker(content::WebContents* web_contents);

  PhishyInteractionTracker(const PhishyInteractionTracker&) = delete;
  PhishyInteractionTracker& operator=(const PhishyInteractionTracker&) = delete;

  ~PhishyInteractionTracker();

  // Records unlogged data if the page is phishy when the WebContents is about
  // to be destroyed.
  void WebContentsDestroyed();

  // Records unlogged data if the page is phishy. Gets called when the primary
  // page is changed.
  void HandlePageChanged();

  // Tracks phishy paste events.
  void HandlePasteEvent();

  // Tracks typing and click events.
  void HandleInputEvent(const blink::WebInputEvent& event);

  // Set the inactivity_delay_ so we can test logged phishy events.
  void SetInactivityDelayForTesting(base::TimeDelta inactivity_delay);

  // Set the UI manager so we can test logged phishy events.
  void SetUIManagerForTesting(
      safe_browsing::SafeBrowsingUIManager* ui_manager_for_testing) {
    ui_manager_for_testing_ = ui_manager_for_testing;
  }

 private:
  // Returns true if the primary page is a phishing page.
  bool IsSitePhishy();

  // Resets values that help track phishy events. Called when the primary page
  // changes.
  void ResetLoggingHelpers();

  // Handles logging for phishy events. Posts a delayed task that logs phishy
  // event data if the user is inactive.
  void HandlePhishyInteraction(
      const ClientSafeBrowsingReportRequest::PhishySiteInteraction::
          PhishySiteInteractionType& interaction);

  // Logs the first event user action. Called on the first occurrence of each
  // type of interaction.
  void RecordFirstInteractionOccurrence(
      ClientSafeBrowsingReportRequest::PhishySiteInteraction::
          PhishySiteInteractionType interaction);

  // Returns true if the user has been inactive on the page for at least
  // inactivity_delay_.
  bool IsUserInactive() {
    return base::Time::Now() - last_interaction_ts_ >= inactivity_delay_;
  }

  // If the user is inactive and the data is unlogged, log the phishy
  // interaction data.
  void MaybeLogIfUserInactive();

  // Helper for logging UMA data.
  void LogPageData();

  // Tracks the WebContents for the current page.
  raw_ptr<content::WebContents> web_contents_ = nullptr;

  // Records the number of occurrences of different user interactions with a
  // phishy page and first/last timestamps of the interaction occurrences. Used
  // for recording metrics.
  PhishySiteInteractionMap phishy_page_interaction_data_;

  // Tracks the latest phishy page interaction occurrence so that we can log
  // metrics after some period of inactivity.
  base::Time last_interaction_ts_;

  // Period of inactivity with a phishy page before we log user interaction
  // metrics.
  base::TimeDelta inactivity_delay_;

  // Used to call a method if the user is inactive for a period of time.
  base::OneShotTimer inactivity_timer_;

  // Tracks the URL so that if metric recording is necessary, we have access to
  // the phishy URL after the page changes.
  GURL current_url_;

  // Tracks the page URL for metric recording.
  GURL current_page_url_;

  // Returns true if the data for the current site has been logged already.
  bool is_data_logged_ = false;

  // Returns true if the current page is phishy.
  bool is_phishy_ = false;

  // UI Manager that returns specific threat types for testing.
  raw_ptr<safe_browsing::SafeBrowsingUIManager, DanglingUntriaged>
      ui_manager_for_testing_ = nullptr;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_PHISHY_INTERACTION_TRACKER_H_
