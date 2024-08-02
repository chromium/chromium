// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_preview_metadata.h"

#include "ash/strings/grit/ash_strings.h"
#include "base/i18n/time_formatting.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {
namespace {

// Taken from //chrome/browser/ash/app_list/search/files/justifications.cc.
// Time limits for how last accessed or modified time maps to each justification
// string.
constexpr base::TimeDelta kJustNow = base::Minutes(15);

std::u16string GetTimeString(base::Time timestamp) {
  const base::Time now = base::Time::Now();
  const base::Time midnight = now.LocalMidnight();
  if ((now - timestamp).magnitude() <= kJustNow) {
    return l10n_util::GetStringUTF16(
        IDS_FILE_SUGGESTION_JUSTIFICATION_TIME_NOW);
  }

  if (timestamp >= midnight && timestamp < midnight + base::Days(1)) {
    return base::TimeFormatTimeOfDay(timestamp);
  }

  return base::LocalizedTimeFormatWithPattern(timestamp, "MMMd");
}

std::u16string GetJustificationString(base::Time viewed, base::Time modified) {
  // Prefer "modified" over "viewed" if they are the same.
  if (modified >= viewed) {
    return l10n_util::GetStringFUTF16(
        IDS_FILE_SUGGESTION_JUSTIFICATION,
        l10n_util::GetStringUTF16(
            IDS_FILE_SUGGESTION_JUSTIFICATION_GENERIC_MODIFIED_ACTION),
        GetTimeString(modified));
  } else {
    return l10n_util::GetStringFUTF16(
        IDS_FILE_SUGGESTION_JUSTIFICATION,
        l10n_util::GetStringUTF16(
            IDS_FILE_SUGGESTION_JUSTIFICATION_YOU_VIEWED_ACTION),
        GetTimeString(viewed));
  }
}

}  // namespace

std::u16string PickerGetFilePreviewDescription(
    std::optional<base::File::Info> info) {
  if (!info.has_value()) {
    return u"";
  }

  if (info->last_modified.is_null() && info->last_accessed.is_null()) {
    return u"";
  }

  return GetJustificationString(info->last_accessed, info->last_modified);
}

}  // namespace ash
