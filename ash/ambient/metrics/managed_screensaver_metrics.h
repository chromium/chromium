// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_METRICS_MANAGED_SCREENSAVER_METRICS_H_
#define ASH_AMBIENT_METRICS_MANAGED_SCREENSAVER_METRICS_H_

#include <string>

#include "ash/ash_export.h"
#include "base/strings/string_piece.h"

namespace ash {

constexpr char kManagedScreensaverEnabledUMA[] = "Enabled";

ASH_EXPORT std::string GetManagedScreensaverHistogram(
    const base::StringPiece& histogram_suffix);

ASH_EXPORT void RecordManagedScreensaverEnabled(bool enabled);

}  // namespace ash

#endif  // ASH_AMBIENT_METRICS_MANAGED_SCREENSAVER_METRICS_H_
