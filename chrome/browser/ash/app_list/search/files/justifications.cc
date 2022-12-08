// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/files/justifications.h"

#include "ash/strings/grit/ash_strings.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/time/time.h"
#include "ui/base/l10n/l10n_util.h"

namespace app_list {
namespace {

// Time limits for how last accessed or modified time maps to each justification
// string.
constexpr base::TimeDelta kJustNow = base::Minutes(15);
constexpr base::TimeDelta kToday = base::Days(1);
constexpr base::TimeDelta kYesterday = base::Days(2);
constexpr base::TimeDelta kPastWeek = base::Days(7);
constexpr base::TimeDelta kPastMonth = base::Days(31);

absl::optional<std::u16string> GetEditStringFromTime(const base::Time& time) {
  const auto& delta = base::Time::Now() - time;
  if (delta <= kJustNow) {
    return l10n_util::GetStringUTF16(IDS_APP_LIST_CONTINUE_EDITED_JUST_NOW);
  } else if (delta <= kToday) {
    return l10n_util::GetStringUTF16(IDS_APP_LIST_CONTINUE_EDITED_TODAY);
  } else if (delta <= kYesterday) {
    return l10n_util::GetStringUTF16(IDS_APP_LIST_CONTINUE_EDITED_YESTERDAY);
  } else if (delta <= kPastWeek) {
    return l10n_util::GetStringUTF16(IDS_APP_LIST_CONTINUE_EDITED_PAST_WEEK);
  } else if (delta <= kPastMonth) {
    return l10n_util::GetStringUTF16(IDS_APP_LIST_CONTINUE_EDITED_PAST_MONTH);
  } else {
    return absl::nullopt;
  }
}

absl::optional<std::u16string> GetOpenStringFromTime(const base::Time& time) {
  const auto& delta = base::Time::Now() - time;
  if (delta <= kJustNow) {
    return l10n_util::GetStringUTF16(IDS_APP_LIST_CONTINUE_OPENED_JUST_NOW);
  } else if (delta <= kToday) {
    return l10n_util::GetStringUTF16(IDS_APP_LIST_CONTINUE_OPENED_TODAY);
  } else if (delta <= kYesterday) {
    return l10n_util::GetStringUTF16(IDS_APP_LIST_CONTINUE_OPENED_YESTERDAY);
  } else if (delta <= kPastWeek) {
    return l10n_util::GetStringUTF16(IDS_APP_LIST_CONTINUE_OPENED_PAST_WEEK);
  } else if (delta <= kPastMonth) {
    return l10n_util::GetStringUTF16(IDS_APP_LIST_CONTINUE_OPENED_PAST_MONTH);
  } else {
    return absl::nullopt;
  }
}

}  // namespace

absl::optional<std::u16string> GetJustificationString(
    const base::Time& last_accessed,
    const base::Time& last_modified) {
  // t1 > t2 means t1 is more recent. When there's a tie, choose modified.
  if (last_modified >= last_accessed) {
    return GetEditStringFromTime(last_modified);
  } else {
    return GetOpenStringFromTime(last_accessed);
  }
}

}  // namespace app_list
