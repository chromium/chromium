// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/files/justifications.h"

#include "ash/strings/grit/ash_strings.h"
#include "base/i18n/icubridge/date_time_formatter.h"
#include "base/i18n/icubridge/icu_bridge.h"
#include "base/i18n/time_formatting.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/ash/file_suggest/file_suggest_util.h"
#include "ui/base/l10n/l10n_util.h"

namespace app_list {
namespace {

// Time limits for how last accessed or modified time maps to each justification
// string.
constexpr base::TimeDelta kJustNow = base::Minutes(15);

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

  return base::i18n::IcuBridge::GetInstance().date_time_formatter().Format(
      timestamp, base::i18n::datetime_options::MD::Medium());
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
  }
  NOTREACHED();
}

}  // namespace

std::optional<std::u16string> GetJustificationString(
    ash::FileSuggestionJustificationType type,
    const base::Time& timestamp,
    const std::string& user_name) {
  return l10n_util::GetStringFUTF16(IDS_FILE_SUGGESTION_JUSTIFICATION,
                                    GetActionString(type, user_name),
                                    GetTimeString(timestamp));
}

}  // namespace app_list
