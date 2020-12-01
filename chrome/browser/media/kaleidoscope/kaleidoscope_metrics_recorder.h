// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_KALEIDOSCOPE_KALEIDOSCOPE_METRICS_RECORDER_H_
#define CHROME_BROWSER_MEDIA_KALEIDOSCOPE_KALEIDOSCOPE_METRICS_RECORDER_H_

#include "base/optional.h"
#include "chrome/browser/media/kaleidoscope/mojom/kaleidoscope.mojom.h"

// Takes care of recording metrics for Kaleidoscope. One of these objects exists
// per KaleidoscopeUI.
class KaleidoscopeMetricsRecorder {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class FirstRunProgress {
    kCompleted = 0,
    kProviderSelection = 1,
    kMediaFeedsConsent = 2,
    kWelcome = 3,
    kMaxValue = kWelcome,
  };

  KaleidoscopeMetricsRecorder();
  KaleidoscopeMetricsRecorder(const KaleidoscopeMetricsRecorder&) = delete;
  KaleidoscopeMetricsRecorder& operator=(const KaleidoscopeMetricsRecorder&) =
      delete;
  ~KaleidoscopeMetricsRecorder();

  // Called when the Kaleidoscope page is exited by the user (e.g. by closing
  // the tab or otherwise navigating.
  void OnExitPage();

  // Called when the user moves to a new step of the FRE.
  void OnFirstRunExperienceStepChanged(
      media::mojom::KaleidoscopeFirstRunExperienceStep step);

 private:
  void RecordFirstRunProgress(FirstRunProgress progress);

  base::Optional<media::mojom::KaleidoscopeFirstRunExperienceStep>
      first_run_experience_step_;
};

#endif  // CHROME_BROWSER_MEDIA_KALEIDOSCOPE_KALEIDOSCOPE_METRICS_RECORDER_H_
