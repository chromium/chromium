// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ANNOTATOR_ANNOTATOR_METRICS_H_
#define ASH_ANNOTATOR_ANNOTATOR_METRICS_H_

namespace ash {

// These enum values represent marker colors on the annotator toolbar and log to
// UMA. Entries should not be renumbered and numeric values should never be
// reused. Please keep in sync with "ProjectorMarkerColor" in
// src/tools/metrics/histograms/metadata/ash/enums.xml.
enum class AnnotatorMarkerColor {
  // kBlack = 0,
  // kWhite = 1,
  kBlue = 2,
  kRed = 3,
  kYellow = 4,
  kMagenta = 5,
  // Add future entries above this comment, in sync with
  // "ProjectorMarkerColor" in
  // src/tools/metrics/histograms/metadata/ash/enums.xml.
  // Update kMaxValue to the last value.
  kMaxValue = kMagenta
};

// Records the marker colors the user chooses to use. Only records if the user
// switches from the default color.
void RecordMarkerColorMetrics(AnnotatorMarkerColor color);

}  // namespace ash

#endif  // ASH_ANNOTATOR_ANNOTATOR_METRICS_H_
