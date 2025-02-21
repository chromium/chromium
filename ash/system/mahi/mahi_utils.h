// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MAHI_MAHI_UTILS_H_
#define ASH_SYSTEM_MAHI_MAHI_UTILS_H_

#include "ash/ash_export.h"

namespace chromeos {
enum class MahiResponseStatus;
}  // namespace chromeos

namespace gfx {
class Rect;
class Size;
}  // namespace gfx

class SkPath;

namespace ash::mahi_utils {

// The `HelpMeReadSettings` policy value is mapped to pref
// `ash::prefs::kHmrManagedSettings`, and the pref value is cast to this enum to
// control the availability of feature and feedback.
// Any integer value outside of this range should be treated equivalently to
// `kAllowedWithoutModelImprovement` (the default for managed users), as
// invalid values can only be achieved if an administrator mistakenly sets it.
enum class HmrEnterprisePolicy {
  kAllowedWithModelImprovement = 0,
  kAllowedWithoutModelImprovement = 1,
  kDisallowed = 2,
};

// Returns the retry link's target visible for `error`.
// NOTE: This function should be called only if the `error` should be presented
// on `MahiErrorStatusView`.
ASH_EXPORT bool CalculateRetryLinkVisible(chromeos::MahiResponseStatus error);

// Returns the text ID of the `error` description on `MahiErrorStatusView`.
// NOTE: This function should be called only if the `error` should be presented
// on `MahiErrorStatusView`.
ASH_EXPORT int GetErrorStatusViewTextId(chromeos::MahiResponseStatus error);

// Checks pref to see if we should show the feedback buttons in the panel.
bool ASH_EXPORT ShouldShowFeedbackButton();

// Gets the cutout clip path and the corner cutout region that will be applied
// to the main view of the Mahi panel, so that feedback buttons will be shown in
// the cutout region.
SkPath ASH_EXPORT GetCutoutClipPath(const gfx::Size& contents_size);
gfx::Rect ASH_EXPORT GetCornerCutoutRegion(const gfx::Rect& contents_bounds);

}  // namespace ash::mahi_utils

#endif  // ASH_SYSTEM_MAHI_MAHI_UTILS_H_
