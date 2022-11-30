// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/recipes/time_format_util.h"

#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

std::string GetViewedItemText(base::Time viewed_time) {
  const auto now = base::Time::Now();
  // Viewing items in the future is not supported. Assume the item was viewed
  // today to account for small shifts between the local and server clock.
  if (viewed_time > now) {
    viewed_time = now;
  }
  const auto viewed_delta = now - viewed_time;

  int message_id = IDS_NTP_MODULES_STATEFUL_TASKS_VIEWED_PREVIOUSLY;
  if (now.LocalMidnight() == viewed_time.LocalMidnight()) {
    message_id = IDS_NTP_MODULES_STATEFUL_TASKS_VIEWED_TODAY;
  } else if ((now - base::Days(1)).LocalMidnight() ==
             viewed_time.LocalMidnight()) {
    message_id = IDS_NTP_MODULES_STATEFUL_TASKS_VIEWED_YESTERDAY;
  } else if (viewed_delta.InDays() < 7) {
    message_id = IDS_NTP_MODULES_STATEFUL_TASKS_VIEWED_PAST_WEEK;
  } else if (viewed_delta.InDays() < 30) {
    message_id = IDS_NTP_MODULES_STATEFUL_TASKS_VIEWED_PAST_MONTH;
  }
  return l10n_util::GetStringUTF8(message_id);
}
