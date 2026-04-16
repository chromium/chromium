// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_METRICS_H_
#define CHROME_BROWSER_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_METRICS_H_

namespace content {
class WebContents;
}  // namespace content

// Entry point of a Click to Call journey.
// These values are logged to UKM. Entries should not be renumbered and numeric
// values should never be reused.
// LINT.IfChange(SharingClickToCallEntryPoint)
enum class SharingClickToCallEntryPoint {
  kLeftClickLink = 0,
  kRightClickLink = 1,
  kRightClickSelection = 2,
  kMaxValue = kRightClickSelection,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:SharingClickToCallEntryPoint)

// Selection at the end of a Click to Call journey.
// These values are logged to UKM. Entries should not be renumbered and numeric
// values should never be reused.
// LINT.IfChange(SharingClickToCallSelection)
enum class SharingClickToCallSelection {
  kNone = 0,
  kDevice = 1,
  kApp = 2,
  kMaxValue = kApp,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:SharingClickToCallSelection)

// Records a Click to Call selection to UKM. This is logged after a completed
// action like selecting an app or a device to send the phone number to.
void LogClickToCallUKM(content::WebContents* web_contents,
                       SharingClickToCallEntryPoint entry_point,
                       bool has_devices,
                       bool has_apps,
                       SharingClickToCallSelection selection);

#endif  // CHROME_BROWSER_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_METRICS_H_
