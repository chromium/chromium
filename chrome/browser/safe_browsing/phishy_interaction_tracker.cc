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
#include "components/safe_browsing/core/common/features.h"
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

PhishyInteractionTracker::PhishyInteractionTracker(
    content::WebContents* web_contents)
    : web_contents_(web_contents), inactivity_delay_(base::Minutes(5)) {
  if (base::FeatureList::IsEnabled(safe_browsing::kAntiPhishingTelemetry)) {
    ResetLoggingHelpers();
  }
}

PhishyInteractionTracker::~PhishyInteractionTracker() {
  if (base::FeatureList::IsEnabled(safe_browsing::kAntiPhishingTelemetry) &&
      is_phishy_ && !is_data_logged_) {
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
  is_phishy_ = IsSitePhishy();
  if (is_phishy_) {
    RecordUserStartsPhishyInteraction();
  }
}

void PhishyInteractionTracker::HandlePasteEvent() {
  if (is_phishy_) {
    HandlePhishyInteraction(PhishyPageInteraction::PHISHY_PASTE_EVENT);
  }
}

void PhishyInteractionTracker::HandleInputEvent(
    const blink::WebInputEvent& event) {
  if (!is_phishy_) {
    return;
  }
  if (event.GetType() == blink::WebInputEvent::Type::kMouseDown) {
    HandlePhishyInteraction(PhishyPageInteraction::PHISHY_CLICK_EVENT);
    return;
  }
#if BUILDFLAG(IS_ANDROID)
  // On Android, key down events are triggered if a user types in through a
  // number bar on Android keyboard. If text is typed in through other parts of
  // Android keyboard, ImeTextCommittedEvent is triggered instead.
  if (event.GetType() == blink::WebInputEvent::Type::kKeyDown) {
    HandlePhishyInteraction(PhishyPageInteraction::PHISHY_KEY_EVENT);
  }
#else   // !BUILDFLAG(IS_ANDROID)
  if (event.GetType() == blink::WebInputEvent::Type::kChar) {
    const blink::WebKeyboardEvent& key_event =
        static_cast<const blink::WebKeyboardEvent&>(event);
    // Key & 0x1f corresponds to the value of the key when either the control or
    // command key is pressed. This detects CTRL+V, COMMAND+V, and CTRL+SHIFT+V.
    if (key_event.windows_key_code == (ui::VKEY_V & 0x1f)) {
      HandlePhishyInteraction(PhishyPageInteraction::PHISHY_PASTE_EVENT);
    } else {
      HandlePhishyInteraction(PhishyPageInteraction::PHISHY_KEY_EVENT);
    }
  }
#endif  // BUILDFLAG(IS_ANDROID)
}

void PhishyInteractionTracker::ResetLoggingHelpers() {
  is_phishy_ = false;
  is_data_logged_ = false;
  new_page_interaction_counts_[PhishyPageInteraction::PHISHY_CLICK_EVENT] = 0;
  new_page_interaction_counts_[PhishyPageInteraction::PHISHY_KEY_EVENT] = 0;
  new_page_interaction_counts_[PhishyPageInteraction::PHISHY_PASTE_EVENT] = 0;
}

bool PhishyInteractionTracker::IsSitePhishy() {
  safe_browsing::SafeBrowsingUIManager* ui_manager_ =
      ui_manager_for_testing_
          ? ui_manager_for_testing_.get()
          : g_browser_process->safe_browsing_service()->ui_manager().get();
  safe_browsing::SBThreatType current_threat_type;
  if (!ui_manager_->IsUrlAllowlistedOrPendingForWebContents(
          web_contents_->GetLastCommittedURL().GetWithEmptyPath(),
          /*is_subresource=*/false,
          web_contents_->GetController().GetLastCommittedEntry(), web_contents_,
          /*allowlist_only=*/true, &current_threat_type)) {
    return false;
  }
  return current_threat_type == safe_browsing::SB_THREAT_TYPE_URL_PHISHING ||
         current_threat_type ==
             safe_browsing::SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING;
}

void PhishyInteractionTracker::HandlePhishyInteraction(
    const PhishyPageInteraction& interaction) {
  last_interaction_ts_ = base::Time::Now();
  // Log if first occurrence of the interaction.
  if (new_page_interaction_counts_[interaction] == 0) {
    RecordFirstInteractionOccurrence(interaction);
  }
  new_page_interaction_counts_[interaction] += 1;
  inactivity_timer_.Start(FROM_HERE, inactivity_delay_, this,
                          &PhishyInteractionTracker::MaybeLogIfUserInactive);
}

void PhishyInteractionTracker::RecordFirstInteractionOccurrence(
    PhishyPageInteraction interaction) {
  switch (interaction) {
    case PhishyPageInteraction::PHISHY_CLICK_EVENT:
      RecordFirstClickEvent();
      break;
    case PhishyPageInteraction::PHISHY_KEY_EVENT:
      RecordFirstKeyEvent();
      break;
    case PhishyPageInteraction::PHISHY_PASTE_EVENT:
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
      new_page_interaction_counts_[PhishyPageInteraction::PHISHY_CLICK_EVENT],
      new_page_interaction_counts_[PhishyPageInteraction::PHISHY_KEY_EVENT],
      new_page_interaction_counts_[PhishyPageInteraction::PHISHY_PASTE_EVENT]);
  is_data_logged_ = true;
}

void PhishyInteractionTracker::SetInactivityDelayForTesting(
    base::TimeDelta inactivity_delay) {
  inactivity_delay_ = inactivity_delay;
}

}  // namespace safe_browsing
