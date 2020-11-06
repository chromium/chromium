// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_metrics.h"

#include "ash/shell.h"
#include "base/metrics/histogram_functions.h"

namespace ash {

namespace {

constexpr char kEntryHistogramName[] = "Ash.CaptureModeController.EntryPoint";
constexpr char kBarButtonHistogramName[] =
    "Ash.CaptureModeController.BarButtons";

// Appends the proper suffix to |prefix| based on whether the user is in tablet
// mode or not.
std::string GetCaptureModeHistogramName(std::string prefix) {
  prefix.append(Shell::Get()->IsInTabletMode() ? ".TabletMode"
                                               : ".ClamshellMode");
  return prefix;
}

}  // namespace

void RecordCaptureModeBarButtonType(CaptureModeBarButtonType button_type) {
  base::UmaHistogramEnumeration(
      GetCaptureModeHistogramName(kBarButtonHistogramName), button_type);
}

void RecordCaptureModeEntryType(CaptureModeEntryType entry_type) {
  base::UmaHistogramEnumeration(
      GetCaptureModeHistogramName(kEntryHistogramName), entry_type);
}

}  // namespace ash
