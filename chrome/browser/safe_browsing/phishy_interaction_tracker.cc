// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/phishy_interaction_tracker.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "components/safe_browsing/content/browser/ui_manager.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace safe_browsing {

namespace {

using base::RecordAction;
using base::UserMetricsAction;

void RecordUserStartsPhishyInteraction() {
  RecordAction(UserMetricsAction("PhishyPage.UserStartsInteraction"));
}

void RecordFirstClickEvent() {
  RecordAction(UserMetricsAction("PhishyPage.FirstClickEvent"));
}

void RecordFirstKeyEvent() {
  RecordAction(UserMetricsAction("PhishyPage.FirstKeyEvent"));
}

void RecordFirstPasteEvent() {
  RecordAction(UserMetricsAction("PhishyPage.FirstPasteEvent"));
}

void RecordFinishedInteractionUMAData(int click_count,
                                      int key_count,
                                      int paste_count) {
  base::UmaHistogramCounts100("SafeBrowsing.PhishySite.ClickEventCount",
                              click_count);
  base::UmaHistogramCounts100("SafeBrowsing.PhishySite.KeyEventCount",
                              key_count);
  base::UmaHistogramCounts100("SafeBrowsing.PhishySite.PasteEventCount",
                              paste_count);
  RecordAction(UserMetricsAction("PhishyPage.UserStopsInteraction"));
}

}  // namespace

PhishyPageInteractionDetails::PhishyPageInteractionDetails(
    int occurrence_count,
    int64_t first_timestamp,
    int64_t last_timestamp)
    : occurrence_count(occurrence_count),
      first_timestamp(first_timestamp),
      last_timestamp(last_timestamp) {}

PhishyInteractionTracker::PhishyInteractionTracker(
    content::WebContents* web_contents)
    : web_contents_(web_contents), inactivity_delay_(base::Minutes(5)) {
  ResetLoggingHelpers();
}

PhishyInteractionTracker::~PhishyInteractionTracker() {
  // If there was any data to log, `WebContentsDestroyed()` should have already
  // handled it.
  DCHECK(!is_phishy_ || is_data_logged_);
}

void PhishyInteractionTracker::WebContentsDestroyed() {
  if (is_phishy_ && !is_data_logged_) {
    LogPageData();
    inactivity_timer_.Stop();
  }
}

void PhishyInteractionTracker::HandlePageChanged() {
  if (is_phishy_ && !is_data_logged_) {
    LogPageData();
  }
  ResetLoggingHelpers();
  inactivity_timer_.Stop();
  current_url_ = web_contents_->GetLastCommittedURL().GetWithEmptyPath();
  current_page_url_ =
      web_contents_->GetController().GetLastCommittedEntry()->GetURL();
  is_phishy_ = IsSitePhishy();
  if (is_phishy_) {
    RecordUserStartsPhishyInteraction();
  }
}

void PhishyInteractionTracker::HandlePasteEvent() {
  if (is_phishy_) {
    HandlePhishyInteraction(ClientSafeBrowsingReportRequest::
                                PhishySiteInteraction::PHISHY_PASTE_EVENT);
  }
}

void PhishyInteractionTracker::HandleInputEvent(
    const blink::WebInputEvent& event) {
  if (!is_phishy_) {
    return;
  }
  if (event.GetType() == blink::WebInputEvent::Type::kMouseDown) {
    HandlePhishyInteraction(ClientSafeBrowsingReportRequest::
                                PhishySiteInteraction::PHISHY_CLICK_EVENT);
    return;
  }
#if BUILDFLAG(IS_ANDROID)
  // On Android, key down events are triggered if a user types in through a
  // number bar on Android keyboard. If text is typed in through other parts of
  // Android keyboard, ImeTextCommittedEvent is triggered instead.
  if (event.GetType() == blink::WebInputEvent::Type::kKeyDown) {
    HandlePhishyInteraction(ClientSafeBrowsingReportRequest::
                                PhishySiteInteraction::PHISHY_KEY_EVENT);
  }
#else   // !BUILDFLAG(IS_ANDROID)
  if (event.GetType() == blink::WebInputEvent::Type::kChar) {
    const blink::WebKeyboardEvent& key_event =
        static_cast<const blink::WebKeyboardEvent&>(event);
    // Key & 0x1f corresponds to the value of the key when either the control or
    // command key is pressed. This detects CTRL+V, COMMAND+V, and CTRL+SHIFT+V.
    if (key_event.windows_key_code == (ui::VKEY_V & 0x1f)) {
      HandlePhishyInteraction(ClientSafeBrowsingReportRequest::
                                  PhishySiteInteraction::PHISHY_PASTE_EVENT);
    } else {
      HandlePhishyInteraction(ClientSafeBrowsingReportRequest::
                                  PhishySiteInteraction::PHISHY_KEY_EVENT);
    }
  }
#endif  // BUILDFLAG(IS_ANDROID)
}

void PhishyInteractionTracker::ResetLoggingHelpers() {
  is_phishy_ = false;
  is_data_logged_ = false;

  phishy_page_interaction_data_.clear();
}

bool PhishyInteractionTracker::IsSitePhishy() {
  if (!ui_manager_for_testing_ && !g_browser_process->safe_browsing_service()) {
    return false;
  }
  safe_browsing::SafeBrowsingUIManager* ui_manager_ =
      ui_manager_for_testing_
          ? ui_manager_for_testing_.get()
          : g_browser_process->safe_browsing_service()->ui_manager().get();
  safe_browsing::SBThreatType current_threat_type;
  if (!ui_manager_->IsUrlAllowlistedOrPendingForWebContents(
          current_url_,
          web_contents_->GetController().GetLastCommittedEntry(), web_contents_,
          /*allowlist_only=*/true, &current_threat_type)) {
    return false;
  }
  return current_threat_type == SBThreatType::SB_THREAT_TYPE_URL_PHISHING ||
         current_threat_type ==
             SBThreatType::SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING ||
         current_threat_type == SBThreatType::SB_THREAT_TYPE_SUSPICIOUS_SITE;
}

void PhishyInteractionTracker::HandlePhishyInteraction(
    const ClientSafeBrowsingReportRequest::PhishySiteInteraction::
        PhishySiteInteractionType& interaction) {
  int new_occurrence_count = 1;
  int64_t new_first_timestamp =
      base::Time::Now().InMillisecondsSinceUnixEpoch();
  int64_t new_last_timestamp = base::Time::Now().InMillisecondsSinceUnixEpoch();
  last_interaction_ts_ = base::Time::Now();
  // Log if first occurrence of the interaction.
  if (!phishy_page_interaction_data_.contains(interaction)) {
    RecordFirstInteractionOccurrence(interaction);
  } else {
    new_occurrence_count +=
        phishy_page_interaction_data_.at(interaction).occurrence_count;
    new_first_timestamp =
        phishy_page_interaction_data_.at(interaction).first_timestamp;
  }
  phishy_page_interaction_data_.insert_or_assign(
      interaction,
      PhishyPageInteractionDetails(new_occurrence_count, new_first_timestamp,
                                   new_last_timestamp));
  inactivity_timer_.Start(FROM_HERE, inactivity_delay_, this,
                          &PhishyInteractionTracker::MaybeLogIfUserInactive);
}

void PhishyInteractionTracker::RecordFirstInteractionOccurrence(
    ClientSafeBrowsingReportRequest::PhishySiteInteraction::
        PhishySiteInteractionType interaction) {
  switch (interaction) {
    case ClientSafeBrowsingReportRequest::PhishySiteInteraction::
        PHISHY_CLICK_EVENT:
      RecordFirstClickEvent();
      break;
    case ClientSafeBrowsingReportRequest::PhishySiteInteraction::
        PHISHY_KEY_EVENT:
      RecordFirstKeyEvent();
      break;
    case ClientSafeBrowsingReportRequest::PhishySiteInteraction::
        PHISHY_PASTE_EVENT:
      RecordFirstPasteEvent();
      break;
    default:
      break;
  }
}

void PhishyInteractionTracker::MaybeLogIfUserInactive() {
  if (IsUserInactive() && !is_data_logged_) {
    LogPageData();
  }
}

void PhishyInteractionTracker::LogPageData() {
  RecordFinishedInteractionUMAData(
      phishy_page_interaction_data_.contains(
          ClientSafeBrowsingReportRequest::PhishySiteInteraction::
              PHISHY_CLICK_EVENT)
          ? phishy_page_interaction_data_
                .at(ClientSafeBrowsingReportRequest::PhishySiteInteraction::
                        PHISHY_CLICK_EVENT)
                .occurrence_count
          : 0,
      phishy_page_interaction_data_.contains(
          ClientSafeBrowsingReportRequest::PhishySiteInteraction::
              PHISHY_KEY_EVENT)
          ? phishy_page_interaction_data_
                .at(ClientSafeBrowsingReportRequest::PhishySiteInteraction::
                        PHISHY_KEY_EVENT)
                .occurrence_count
          : 0,
      phishy_page_interaction_data_.contains(
          ClientSafeBrowsingReportRequest::PhishySiteInteraction::
              PHISHY_PASTE_EVENT)
          ? phishy_page_interaction_data_
                .at(ClientSafeBrowsingReportRequest::PhishySiteInteraction::
                        PHISHY_PASTE_EVENT)
                .occurrence_count
          : 0);
  // Send PHISHY_SITE_INTERACTIONS report for relevant OSes.
#if BUILDFLAG(FULL_SAFE_BROWSING)
  g_browser_process->safe_browsing_service()->SendPhishyInteractionsReport(
      Profile::FromBrowserContext(web_contents_->GetBrowserContext()),
      current_url_, current_page_url_, phishy_page_interaction_data_);
#endif
  is_data_logged_ = true;
}

void PhishyInteractionTracker::SetInactivityDelayForTesting(
    base::TimeDelta inactivity_delay) {
  inactivity_delay_ = inactivity_delay;
}

}  // namespace safe_browsing
