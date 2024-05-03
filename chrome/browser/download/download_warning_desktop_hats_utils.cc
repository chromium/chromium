// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_warning_desktop_hats_utils.h"

#include <iterator>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/version_info/channel.h"
#include "build/build_config.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_item_warning_data.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/pref_names.h"
#include "components/download/public/common/download_item.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/download_item_utils.h"

namespace {

bool IsDownloadBubbleTrigger(DownloadWarningHatsType type) {
  return type == DownloadWarningHatsType::kDownloadBubbleBypass ||
         type == DownloadWarningHatsType::kDownloadBubbleHeed ||
         type == DownloadWarningHatsType::kDownloadBubbleIgnore;
}

bool IsDownloadsPageTrigger(DownloadWarningHatsType type) {
  return !IsDownloadBubbleTrigger(type);
}

std::string GetOutcomeStringData(DownloadWarningHatsType type) {
  switch (type) {
    case DownloadWarningHatsType::kDownloadBubbleBypass:
    case DownloadWarningHatsType::kDownloadsPageBypass:
      return "Bypassed warning";
    case DownloadWarningHatsType::kDownloadBubbleHeed:
    case DownloadWarningHatsType::kDownloadsPageHeed:
      return "Heeded warning";
    case DownloadWarningHatsType::kDownloadBubbleIgnore:
    case DownloadWarningHatsType::kDownloadsPageIgnore:
      return "Ignored warning";
  }
}

std::string GetSurfaceStringData(DownloadWarningHatsType type) {
  return IsDownloadBubbleTrigger(type) ? "Download bubble" : "Downloads page";
}

std::string GetDangerTypeStringData(const DownloadItemModel& model) {
  std::string danger_type_string =
      GetDownloadDangerTypeString(model.GetDangerType());

  switch (model.GetTailoredWarningType()) {
    case DownloadUIModel::TailoredWarningType::kNoTailoredWarning:
      break;
    case DownloadUIModel::TailoredWarningType::kCookieTheft:
      base::StrAppend(&danger_type_string, {", Cookie theft"});
      break;
    case DownloadUIModel::TailoredWarningType::kCookieTheftWithAccountInfo:
      base::StrAppend(&danger_type_string,
                      {", Cookie theft with account info"});
      break;
    case DownloadUIModel::TailoredWarningType::kSuspiciousArchive:
      base::StrAppend(&danger_type_string, {", Suspicious archive"});
      break;
  }

  return danger_type_string;
}

std::string GetWarningTypeStringData(const DownloadUIModel& model) {
  switch (model.GetDangerUiPattern()) {
    case DownloadUIModel::DangerUiPattern::kNormal:
    case DownloadUIModel::DangerUiPattern::kOther:
      return "None";
    case DownloadUIModel::DangerUiPattern::kDangerous:
      return "Dangerous";
    case DownloadUIModel::DangerUiPattern::kSuspicious:
      return "Suspicious";
  }
}

std::string ElapsedTimeToSecondsString(base::TimeDelta elapsed_time) {
  return base::NumberToString(elapsed_time.InSeconds());
}

std::string SafeBrowsingStateToString(
    safe_browsing::SafeBrowsingState sb_state) {
  switch (sb_state) {
    case safe_browsing::SafeBrowsingState::NO_SAFE_BROWSING:
      return "No Safe Browsing";
    case safe_browsing::SafeBrowsingState::STANDARD_PROTECTION:
      return "Standard Protection";
    case safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION:
      return "Enhanced Protection";
  }
}

// Produces a string consisting of comma-separated action events, each of which
// consists of the surface, action, and relative timestamp (ms) separated by
// colons. The first SHOWN event is included, and is the basis of all
// timestamps, however subsequent SHOWN events are not included.
std::string SerializeWarningActionEvents(
    DownloadItemWarningData::WarningSurface warning_first_shown_surface,
    const std::vector<DownloadItemWarningData::WarningActionEvent>& events) {
  // The first SHOWN event is not stored by DownloadItemWarningData, so we
  // construct it here.
  std::string first_event_string =
      DownloadItemWarningData::WarningActionEvent{
          warning_first_shown_surface,
          DownloadItemWarningData::WarningAction::SHOWN, 0,
          /*is_terminal_action=*/false}
          .ToString();

  std::vector<std::string> event_strings;
  event_strings.reserve(events.size() + 1);
  event_strings.push_back(std::move(first_event_string));
  base::ranges::transform(
      events.begin(), events.end(), std::back_inserter(event_strings),
      [](const DownloadItemWarningData::WarningActionEvent& event) {
        return event.ToString();
      });

  return base::JoinString(std::move(event_strings), ",");
}

}  // namespace

DownloadWarningHatsProductSpecificData::DownloadWarningHatsProductSpecificData(
    DownloadWarningHatsType survey_type)
    : survey_type_(survey_type) {}

DownloadWarningHatsProductSpecificData::DownloadWarningHatsProductSpecificData(
    const DownloadWarningHatsProductSpecificData&) = default;

DownloadWarningHatsProductSpecificData&
DownloadWarningHatsProductSpecificData::operator=(
    const DownloadWarningHatsProductSpecificData&) = default;

DownloadWarningHatsProductSpecificData::DownloadWarningHatsProductSpecificData(
    DownloadWarningHatsProductSpecificData&&) = default;

DownloadWarningHatsProductSpecificData&
DownloadWarningHatsProductSpecificData::operator=(
    DownloadWarningHatsProductSpecificData&&) = default;

DownloadWarningHatsProductSpecificData::
    ~DownloadWarningHatsProductSpecificData() = default;

// static
DownloadWarningHatsProductSpecificData
DownloadWarningHatsProductSpecificData::Create(
    DownloadWarningHatsType survey_type,
    download::DownloadItem* download_item) {
  CHECK(download_item);

  DownloadWarningHatsProductSpecificData psd{survey_type};

  psd.string_data_.insert(
      {Fields::kChannel,
       std::string(version_info::GetChannelString(chrome::GetChannel()))});

  psd.string_data_.insert(
      {Fields::kOutcome, GetOutcomeStringData(survey_type)});
  psd.string_data_.insert(
      {Fields::kSurface, GetSurfaceStringData(survey_type)});

  psd.string_data_.insert(
      {Fields::kSecondsSinceDownloadStarted,
       ElapsedTimeToSecondsString(base::Time::Now() -
                                  download_item->GetStartTime())});

  base::Time warning_shown_time =
      DownloadItemWarningData::WarningFirstShownTime(download_item);
  if (!warning_shown_time.is_null()) {
    psd.string_data_.insert(
        {Fields::kSecondsSinceWarningShown,
         ElapsedTimeToSecondsString(base::Time::Now() - warning_shown_time)});
  }

  psd.bits_data_.insert(
      {Fields::kUserGesture, download_item->HasUserGesture()});

  // This won't be used for generating any strings, so it's ok not to use the
  // correct StatusTextBuilder.
  DownloadItemModel download_model(download_item);

  psd.string_data_.insert(
      {Fields::kDangerType, GetDangerTypeStringData(download_model)});
  psd.string_data_.insert(
      {Fields::kWarningType, GetWarningTypeStringData(download_model)});

  // TODO(chlily): Implement kIgnoreTimeout.

  // Assemble the Profile-dependent PSD.
  Profile* profile = Profile::FromBrowserContext(
      content::DownloadItemUtils::GetBrowserContext(download_item));
  if (!profile) {
    return psd;
  }

  psd.string_data_.insert(
      {Fields::kSafeBrowsingState,
       SafeBrowsingStateToString(
           safe_browsing::GetSafeBrowsingState(*profile->GetPrefs()))});

  psd.bits_data_.insert({Fields::kPartialViewEnabled,
                         profile->GetPrefs()->GetBoolean(
                             prefs::kDownloadBubblePartialViewEnabled)});

  // URL and filename logged only for Safe Browsing users.
  if (safe_browsing::IsSafeBrowsingEnabled(*profile->GetPrefs())) {
    psd.string_data_.insert({Fields::kUrlDownload,
                             download_item->GetURL().possibly_invalid_spec()});
    psd.string_data_.insert(
        {Fields::kUrlReferrer,
         download_item->GetReferrerUrl().possibly_invalid_spec()});
    psd.string_data_.insert(
        {Fields::kFilename,
         base::UTF16ToUTF8(
             download_item->GetFileNameToReportUser().LossyDisplayName())});
  }

  // Interaction details logged only for ESB users.
  std::optional<DownloadItemWarningData::WarningSurface>
      warning_first_shown_surface =
          DownloadItemWarningData::WarningFirstShownSurface(download_item);
  if (warning_first_shown_surface &&
      safe_browsing::IsEnhancedProtectionEnabled(*profile->GetPrefs())) {
    std::vector<DownloadItemWarningData::WarningActionEvent>
        warning_action_events =
            DownloadItemWarningData::GetWarningActionEvents(download_item);
    psd.string_data_.insert(
        {Fields::kWarningInteractions,
         SerializeWarningActionEvents(*warning_first_shown_surface,
                                      warning_action_events)});
  }

  return psd;
}

void DownloadWarningHatsProductSpecificData::AddNumPageWarnings(int num) {
  if (IsDownloadsPageTrigger(survey_type_)) {
    string_data_.insert({Fields::kNumPageWarnings, base::NumberToString(num)});
  }
}

void DownloadWarningHatsProductSpecificData::AddPartialViewInteraction(
    bool partial_view_interaction) {
  if (IsDownloadBubbleTrigger(survey_type_)) {
    bits_data_.insert(
        {Fields::kPartialViewInteraction, partial_view_interaction});
  }
}

// static
std::vector<std::string>
DownloadWarningHatsProductSpecificData::GetBitsDataFields() {
  return {Fields::kPartialViewEnabled, Fields::kPartialViewInteraction,
          Fields::kUserGesture};
}

// static
std::vector<std::string>
DownloadWarningHatsProductSpecificData::GetStringDataFields() {
  return {
      Fields::kOutcome,
      Fields::kSurface,
      Fields::kDangerType,
      Fields::kWarningType,
      Fields::kSafeBrowsingState,
      Fields::kChannel,
      Fields::kNumPageWarnings,
      Fields::kWarningInteractions,
      Fields::kSecondsSinceDownloadStarted,
      Fields::kSecondsSinceWarningShown,
      Fields::kUrlDownload,
      Fields::kUrlReferrer,
      Fields::kFilename,
      // TODO(chlily): Add kIgnoreTimeout.
  };
}

std::optional<std::string> MaybeGetDownloadWarningHatsTrigger(
    DownloadWarningHatsType survey_type) {
  if (!base::FeatureList::IsEnabled(safe_browsing::kDownloadWarningSurvey)) {
    return std::nullopt;
  }

  const int eligible_survey_type =
      safe_browsing::kDownloadWarningSurveyType.Get();

  // Configuration error.
  if (eligible_survey_type < 0 ||
      eligible_survey_type >
          static_cast<int>(DownloadWarningHatsType::kMaxValue)) {
    return std::nullopt;
  }

  // User is not assigned to be eligible for this type.
  if (static_cast<DownloadWarningHatsType>(eligible_survey_type) !=
      survey_type) {
    return std::nullopt;
  }

  switch (survey_type) {
    case DownloadWarningHatsType::kDownloadBubbleBypass:
      return kHatsSurveyTriggerDownloadWarningBubbleBypass;
    case DownloadWarningHatsType::kDownloadBubbleHeed:
      return kHatsSurveyTriggerDownloadWarningBubbleHeed;
    case DownloadWarningHatsType::kDownloadBubbleIgnore:
      return kHatsSurveyTriggerDownloadWarningBubbleIgnore;
    case DownloadWarningHatsType::kDownloadsPageBypass:
      return kHatsSurveyTriggerDownloadWarningPageBypass;
    case DownloadWarningHatsType::kDownloadsPageHeed:
      return kHatsSurveyTriggerDownloadWarningPageHeed;
    case DownloadWarningHatsType::kDownloadsPageIgnore:
      return kHatsSurveyTriggerDownloadWarningPageIgnore;
  }
}
