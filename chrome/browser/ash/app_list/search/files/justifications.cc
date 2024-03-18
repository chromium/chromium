// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/files/justifications.h"

#include "ash/constants/ash_features.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/ash/file_suggest/file_suggest_util.h"
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

std::u16string GetTimeString(const base::Time& timestamp) {
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

std::optional<std::u16string> GetEditStringFromTime(const base::Time& time) {
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
    return std::nullopt;
  }
}

std::optional<std::u16string> GetOpenStringFromTime(const base::Time& time) {
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
    return std::nullopt;
  }
}

std::u16string GetActionString(ash::FileSuggestionJustificationType type,
                               const std::string& user_name) {
  switch (type) {
    case ash::FileSuggestionJustificationType::kViewed: {
      return l10n_util::GetStringUTF16(
          IDS_FILE_SUGGESTION_JUSTIFICATION_YOU_VIEWED_ACTION);
    }
    case ash::FileSuggestionJustificationType::kModified: {
      if (user_name.empty()) {
        return l10n_util::GetStringUTF16(
            IDS_FILE_SUGGESTION_JUSTIFICATION_GENERIC_MODIFIED_ACTION);
      }
      return l10n_util::GetStringFUTF16(
          IDS_FILE_SUGGESTION_JUSTIFICATION_USER_MODIFIED_ACTION,
          base::UTF8ToUTF16(user_name));
    }
    case ash::FileSuggestionJustificationType::kModifiedByCurrentUser: {
      return l10n_util::GetStringUTF16(
          IDS_FILE_SUGGESTION_JUSTIFICATION_YOU_MODIFIED_ACTION);
    }
    case ash::FileSuggestionJustificationType::kShared: {
      if (user_name.empty()) {
        return l10n_util::GetStringUTF16(
            IDS_FILE_SUGGESTION_JUSTIFICATION_GENERIC_SHARED_ACTION);
      }
      return l10n_util::GetStringFUTF16(
          IDS_FILE_SUGGESTION_JUSTIFICATION_USER_SHARED_ACTION,
          base::UTF8ToUTF16(user_name));
    }
    case ash::FileSuggestionJustificationType::kUnknown: {
      return u"";
    }
  }
}

}  // namespace

std::optional<std::u16string> GetJustificationString(
    ash::FileSuggestionJustificationType type,
    const base::Time& timestamp,
    const std::string& user_name) {
  if (ash::features::IsLauncherContinueSectionWithRecentsEnabled()) {
    return l10n_util::GetStringFUTF16(IDS_FILE_SUGGESTION_JUSTIFICATION,
                                      GetActionString(type, user_name),
                                      GetTimeString(timestamp));
  }
  switch (type) {
    case ash::FileSuggestionJustificationType::kViewed:
      return GetOpenStringFromTime(timestamp);
    case ash::FileSuggestionJustificationType::kModified:
    case ash::FileSuggestionJustificationType::kModifiedByCurrentUser:
      return GetEditStringFromTime(timestamp);
    case ash::FileSuggestionJustificationType::kShared:
    case ash::FileSuggestionJustificationType::kUnknown:
      return std::nullopt;
  }
}

}  // namespace app_list
