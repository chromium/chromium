// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_SRT_FIELD_TRIAL_WIN_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_SRT_FIELD_TRIAL_WIN_H_

#include <string>

#include "base/feature_list.h"
#include "url/gurl.h"

namespace safe_browsing {

// These values are used to send UMA information and are replicated in the
// histograms.xml file, so the order MUST NOT CHANGE.
enum SRTPromptHistogramValue {
  SRT_PROMPT_SHOWN = 0,
  SRT_PROMPT_ACCEPTED = 1,
  SRT_PROMPT_DENIED = 2,
  SRT_PROMPT_FALLBACK = 3,
  SRT_PROMPT_DOWNLOAD_UNAVAILABLE = 4,
  SRT_PROMPT_CLOSED = 5,
  SRT_PROMPT_SHOWN_FROM_MENU = 6,

  SRT_PROMPT_MAX,
};

// These values are used to send UMA information and are replicated in the
// histograms.xml file, so the order MUST NOT CHANGE.
enum NoPromptReasonHistogramValue {
  NO_PROMPT_REASON_BEHAVIOUR_NOT_SUPPORTED = 0,
  NO_PROMPT_REASON_FEATURE_NOT_ENABLED = 1,
  NO_PROMPT_REASON_NOTHING_FOUND = 2,
  NO_PROMPT_REASON_ALREADY_PROMPTED = 3,
  NO_PROMPT_REASON_CLEANER_DOWNLOAD_FAILED = 4,
  NO_PROMPT_REASON_BROWSER_NOT_AVAILABLE = 5,
  NO_PROMPT_REASON_NOT_ON_IDLE_STATE = 6,
  NO_PROMPT_REASON_IPC_CONNECTION_BROKEN = 7,
  NO_PROMPT_REASON_WAITING_FOR_BROWSER = 8,

  NO_PROMPT_REASON_MAX,
};

// These values are used to send UMA information about the histogram type
// and are replicated in the histograms.xml file, so the order MUST NOT CHANGE.
enum PromptTypeHistogramValue {
  PROMPT_TYPE_LEGACY_PROMPT_SHOWN = 0,
  PROMPT_TYPE_ON_TRANSITION_TO_INFECTED_STATE = 1,
  PROMPT_TYPE_ON_BROWSER_WINDOW_AVAILABLE = 2,

  PROMPT_TYPE_MAX,
};

// Feature to control whether users should be prompted for a cleanup if the
// software reporter finds bad software on their machine.
extern const base::Feature kChromeCleanupInBrowserPromptFeature;

// Feature, parameters of which control which software reporter and cleanup tool
// versions will be downloaded. When not enabled, default versions will be used.
extern const base::Feature kChromeCleanupDistributionFeature;

// Extensions cleanup feature. When enabled, Chrome Cleaner will prompt users
// for, and cleanup, bad extensions.
extern const base::Feature kChromeCleanupExtensionsFeature;

// Returns true if this Chrome is in a field trial group which shows the SRT
// prompt.
bool IsSRTPromptFeatureEnabled();

// Returns the correct SRT download URL for the current field trial.
GURL GetSRTDownloadURL();

// Returns the value of the incoming SRT seed.
std::string GetIncomingSRTSeed();

// Returns the value of the "Group" parameter associed with the SRTPrompt
// feature.
std::string GetSRTPromptGroupName();

// Records a value for the SRT Prompt Histogram.
void RecordSRTPromptHistogram(SRTPromptHistogramValue value);

// Records a value for SoftwareReporter.PromptShownWithType Histogram
void RecordPromptShownWithTypeHistogram(PromptTypeHistogramValue value);

// Records a SoftwareReporter.PromptShown histogram with value false and
// a SoftwareReporter.NoPromptReason histogram with the reason corresponding
// to |value|.
void RecordPromptNotShownWithReasonHistogram(
    NoPromptReasonHistogramValue value);

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_SRT_FIELD_TRIAL_WIN_H_
