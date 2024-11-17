// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/mahi/mahi_utils.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/mahi/mahi_constants.h"
#include "ash/utility/arc_curve_path_util.h"
#include "base/notreached.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "components/prefs/pref_service.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace ash::mahi_utils {

bool CalculateRetryLinkVisible(chromeos::MahiResponseStatus error) {
  switch (error) {
    case chromeos::MahiResponseStatus::kCantFindOutputData:
    case chromeos::MahiResponseStatus::kContentExtractionError:
    case chromeos::MahiResponseStatus::kInappropriate:
    case chromeos::MahiResponseStatus::kUnknownError:
      return true;
    case chromeos::MahiResponseStatus::kQuotaLimitHit:
    case chromeos::MahiResponseStatus::kResourceExhausted:
    case chromeos::MahiResponseStatus::kRestrictedCountry:
    case chromeos::MahiResponseStatus::kUnsupportedLanguage:
      return false;
    case chromeos::MahiResponseStatus::kLowQuota:
    case chromeos::MahiResponseStatus::kSuccess:
      NOTREACHED();
  }
}

int GetErrorStatusViewTextId(chromeos::MahiResponseStatus error) {
  switch (error) {
    case chromeos::MahiResponseStatus::kCantFindOutputData:
    case chromeos::MahiResponseStatus::kContentExtractionError:
    case chromeos::MahiResponseStatus::kUnknownError:
      return IDS_ASH_MAHI_ERROR_STATUS_LABEL_GENERAL;
    case chromeos::MahiResponseStatus::kInappropriate:
      return IDS_ASH_MAHI_RESPONSE_STATUS_INAPPROPRIATE_LABEL_TEXT;
    case chromeos::MahiResponseStatus::kQuotaLimitHit:
    case chromeos::MahiResponseStatus::kResourceExhausted:
      return IDS_ASH_MAHI_ERROR_STATUS_LABEL_AT_CAPACITY;
    case chromeos::MahiResponseStatus::kRestrictedCountry:
      return IDS_ASH_MAHI_ERROR_STATUS_LABEL_RESTRICTED_COUNTRY;
    case chromeos::MahiResponseStatus::kUnsupportedLanguage:
      return IDS_ASH_MAHI_ERROR_STATUS_LABEL_UNSUPPORTED_LANGUAGE;
    case chromeos::MahiResponseStatus::kLowQuota:
    case chromeos::MahiResponseStatus::kSuccess:
      NOTREACHED();
    default:
      // TOOD(b/343472496): Remove this when UI is added.
      return IDS_ASH_MAHI_ERROR_STATUS_LABEL_GENERAL;
  }
}

bool ShouldShowFeedbackButton() {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetActivePrefService();

  // PrefService might be null in tests. In that case the feedback buttons
  // should be shown by default.
  return prefs ? prefs->GetBoolean(prefs::kHmrFeedbackAllowed) : true;
}

SkPath GetCutoutClipPath(const gfx::Size& contents_size) {
  return ShouldShowFeedbackButton()
             ? util::GetArcCurveRectPath(
                   contents_size,
                   util::ArcCurveCorner(
                       util::ArcCurveCorner::CornerLocation::kBottomRight,
                       gfx::Size(mahi_constants::kCutoutWidth +
                                     mahi_constants::kCutoutConvexRadius,
                                 mahi_constants::kCutoutHeight +
                                     mahi_constants::kCutoutConvexRadius),
                       mahi_constants::kCutoutConcaveRadius,
                       mahi_constants::kCutoutConvexRadius),
                   mahi_constants::kContentScrollViewCornerRadius)
             : util::GetArcCurveRectPath(
                   contents_size,
                   mahi_constants::kContentScrollViewCornerRadius);
}

gfx::Rect GetCornerCutoutRegion(const gfx::Rect& contents_bounds) {
  return gfx::Rect(contents_bounds.width() - mahi_constants::kCutoutWidth,
                   contents_bounds.height() - mahi_constants::kCutoutHeight,
                   mahi_constants::kCutoutWidth, mahi_constants::kCutoutHeight);
}

}  // namespace ash::mahi_utils
