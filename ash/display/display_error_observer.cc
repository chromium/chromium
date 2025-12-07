// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_error_observer.h"

#include <string>

#include "ash/display/display_util.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/display/display_features.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/util/display_util.h"

namespace ash {

DisplayErrorObserver::DisplayErrorObserver() = default;

DisplayErrorObserver::~DisplayErrorObserver() = default;

void DisplayErrorObserver::OnDisplayConfigurationChangeFailed(
    const display::DisplayConfigurator::DisplayStateList& displays,
    display::MultipleDisplayState new_state) {
  bool internal_display_failed = false;
  int num_external = 0;
  LOG(ERROR) << "Failed to configure the following display(s):";
  for (display::DisplaySnapshot* display : displays) {
    const int64_t display_id = display->display_id();
    bool is_internal = display::IsInternalDisplayId(display_id);
    internal_display_failed |= is_internal;
    if (!is_internal) {
      ++num_external;
    }
    LOG(ERROR) << "- Display with ID = " << display_id
               << ", and EDID = " << base::HexEncode(display->edid()) << ".";
  }

  if (internal_display_failed && displays.size() == 1u) {
    // If the internal display is the only display that failed, don't show this
    // notification to the user, as it's confusing and less helpful.
    // https://crbug.com/775197.
    return;
  }

  if (display::features::IsMaxExternalDisplaySupportedNotificationEnabled()) {
    const int display_limit =
        display::features::kMaxExternalDisplaySupportedNotificationLimit.Get();
    if (num_external > display_limit) {
      std::u16string limit_message =
          l10n_util::GetStringFUTF16(IDS_ASH_DISPLAY_FAILURE_MAX_DISPLAY_LIMIT,
                                     base::NumberToString16(display_limit));
      ShowDisplayErrorNotification(limit_message, true);
      return;
    }
  }

  std::u16string message =
      (new_state == display::MULTIPLE_DISPLAY_STATE_MULTI_MIRROR)
          ? l10n_util::GetStringUTF16(IDS_ASH_DISPLAY_FAILURE_ON_MIRRORING)
          : ui::SubstituteChromeOSDeviceType(
                IDS_ASH_DISPLAY_FAILURE_ON_NON_MIRRORING);
  ShowDisplayErrorNotification(message, true);
}

}  // namespace ash
