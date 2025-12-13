// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEEDBACK_PUBLIC_FEEDBACK_SOURCE_H_
#define CHROME_BROWSER_FEEDBACK_PUBLIC_FEEDBACK_SOURCE_H_

// Sources of feedback requests.
//
// WARNING: The below enum values MUST never be modified or reordered, as
// they're written to logs. You can only insert a new element immediately
// before the last, or deprecate existing values. Also, 'FeedbackSource' in
// 'tools/metrics/histograms/enums.xml' MUST be kept in sync with the enum
// below.
namespace feedback {

// LINT.IfChange(FeedbackSource)
enum FeedbackSource {
  kFeedbackSourceArcApp = 0,
  kFeedbackSourceAsh = 1,
  kFeedbackSourceBrowserCommand = 2,
  kFeedbackSourceMdSettingsAboutPage = 3,
  // kFeedbackSourceOldSettingsAboutPage = 4, Obsolete
  kFeedbackSourceProfileErrorDialog = 5,
  kFeedbackSourceSadTabPage = 6,
  // kFeedbackSourceSupervisedUserInterstitial = 7, Obsolete
  kFeedbackSourceAssistant = 8,
  kFeedbackSourceDesktopTabGroups = 9,
  kFeedbackSourceMediaApp = 10,
  kFeedbackSourceHelpApp = 11,
  // kFeedbackSourceKaleidoscope = 12, Obsolete
  kFeedbackSourceNetworkHealthPage = 13,
  // kFeedbackSourceTabSearch = 14, Obsolete
  kFeedbackSourceCameraApp = 15,
  // kFeedbackSourceCaptureMode = 16, Obsolete
  kFeedbackSourceChromeLabs = 17,
  // kFeedbackSourceBentoBar = 18, Obsolete
  kFeedbackSourceQuickAnswers = 19,
  // kFeedbackSourceWhatsNew = 20, Obsolete
  kFeedbackSourceConnectivityDiagnostics = 21,
  kFeedbackSourceProjectorApp = 22,
  // kFeedbackSourceDesksTemplates = 23, Obsolete
  kFeedbackSourceFilesApp = 24,
  kFeedbackSourceChannelIndicator = 25,
  kFeedbackSourceLauncher = 26,
  kFeedbackSourceSettingsPerformancePage = 27,
  kFeedbackSourceQuickOffice = 28,
  kFeedbackSourceOsSettingsSearch = 29,
  kFeedbackSourceAutofillContextMenu = 30,
  // kFeedbackSourceUnknownLacrosSource = 31, Obsolete
  kFeedbackSourceWindowLayoutMenu = 32,
  kFeedbackSourcePriceInsights = 33,
  kFeedbackSourceCookieControls = 34,
  kFeedbackSourceGameDashboard = 35,
  kFeedbackSourceLensOverlay = 36,
  kFeedbackSourceLogin = 37,
  kFeedbackSourceAI = 38,
  // kFeedbackSourceFocusMode = 39, Obsolete
  kFeedbackSourceOverview = 40,
  // kFeedbackSourceSnapGroups = 41, Obsolete
  // kFeedbackSourceBirch = 42, Obsolete
  kFeedbackSourceBorealis = 43,
  kFeedbackSourceSunfish = 44,
  kFeedbackSourceBocaApp = 45,
  kFeedbackSourceTrackingProtections = 46,
  kFeedbackSourceSplitView = 47,

  // ATTENTION: Before making any changes or adding to feedback collection,
  // please ensure the teams that operationalize feedback are aware and
  // supportive. Contact: chrome-gtech@

  // Must be last.
  kFeedbackSourceCount,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:FeedbackSource)

}  // namespace feedback

#endif  // CHROME_BROWSER_FEEDBACK_PUBLIC_FEEDBACK_SOURCE_H_
