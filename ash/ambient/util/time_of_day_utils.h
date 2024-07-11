// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_UTIL_TIME_OF_DAY_UTILS_H_
#define ASH_AMBIENT_UTIL_TIME_OF_DAY_UTILS_H_

#include <string>

#include "ash/ash_export.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/time/time.h"

namespace ash {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// See enum AmbientDlcError in tools/metrics/histograms/ash/enums.xml.
enum class DlcError {
  kUnknown = 0,
  kNone = 1,
  kInternal = 2,
  kBusy = 3,
  kNeedReboot = 4,
  kInvalidDlc = 5,
  kAllocation = 6,
  kNoImageFound = 7,
  kMaxValue = kNoImageFound,
};

// Returns full path to the html required to render the TimeOfDay screen saver.
// The returned path will be empty if an error occurred and the html is
// temporarily unavailable.
//
// `dlc_metrics_label` is part of the UMA metric name
// ("Ash.AmbientMode.VideoDlcInstall.<dlc_metrics_label>.Error") that tracks the
// success/failure of the DLC installation performed within this function.
ASH_EXPORT void GetAmbientVideoHtmlPath(
    std::string dlc_metrics_label,
    base::OnceCallback<void(base::FilePath)> on_done);

// Installs the ambient video DLC package silently in the background.
//
// The background install increases the probability of a successful DLC install
// happening before the video screen saver is launched. If it fails, another
// installation attempt is made in the foreground when it's time to launch the
// screen saver. After one successful install, all future installation requests
// will be successful and simpler since the resources have already been
// downloaded and persisted on device.
ASH_EXPORT void InstallAmbientVideoDlcInBackground();

// TimeOfDay video file names.
ASH_EXPORT extern const base::FilePath::CharType kTimeOfDayCloudsVideo[];
ASH_EXPORT extern const base::FilePath::CharType kTimeOfDayNewMexicoVideo[];
ASH_EXPORT extern const base::FilePath::CharType kTimeOfDayVideoHtmlSubPath[];

}  // namespace ash

#endif  // ASH_AMBIENT_UTIL_TIME_OF_DAY_UTILS_H_
