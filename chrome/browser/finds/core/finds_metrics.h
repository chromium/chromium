// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FINDS_CORE_FINDS_METRICS_H_
#define CHROME_BROWSER_FINDS_CORE_FINDS_METRICS_H_

namespace finds {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(FindsNotificationUserInteraction)
enum class FindsNotificationUserInteraction {
  kClick = 0,
  kDismiss = 1,
  kHelpfulButtonClick = 2,
  kUnhelpfulButtonClick = 3,
  kMaxValue = kUnhelpfulButtonClick,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/notifications/enums.xml:FindsNotificationUserInteraction)

// Record when a finds notification is shown.
void RecordNotificationShown();
// Record a histogram tracking the type of user interaction with a finds
// notification.
void RecordNotificationInteraction(
    FindsNotificationUserInteraction interaction);

}  // namespace finds

#endif  // CHROME_BROWSER_FINDS_CORE_FINDS_METRICS_H_
