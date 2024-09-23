// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_warning_desktop_hats_utils.h"

#include <cstdint>
#include <iterator>
#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/version_info/channel.h"
#include "build/build_config.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_item_warning_data.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
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

// Placeholder strings for fields that are not logged.
constexpr char kNotAvailable[] = "Not available";
constexpr char kNotLoggedNoSafeBrowsing[] =
    "Not logged because Safe Browsing is off";
constexpr char kNotLoggedNoEnhancedProtection[] =
    "Not logged because Enhanced Protection is off";

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
  CHECK(CanShowDownloadWarningHatsSurvey(download_item));

  DownloadWarningHatsProductSpecificData psd{survey_type};

  // Add placeholders for the fields that must be added later, to avoid CHECKing
  // even if they are forgotten.
  if (IsDownloadBubbleTrigger(survey_type)) {
    psd.bits_data_.insert({Fields::kPartialViewInteraction, false});
  }
  if (IsDownloadsPageTrigger(survey_type)) {
    psd.string_data_.insert({Fields::kNumPageWarnings, kNotAvailable});
  }

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
  } else {
    psd.string_data_.insert({Fields::kSecondsSinceWarningShown, kNotAvailable});
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

  if (survey_type == DownloadWarningHatsType::kDownloadBubbleIgnore) {
    psd.string_data_.insert(
        {Fields::kIgnoreTimeoutSeconds,
         ElapsedTimeToSecondsString(GetIgnoreDownloadBubbleWarningDelay())});
  }

  // Assemble the Profile-dependent PSD.
  Profile* profile = Profile::FromBrowserContext(
      content::DownloadItemUtils::GetBrowserContext(download_item));
  if (!profile) {
    psd.string_data_.insert({Fields::kSafeBrowsingState, kNotAvailable});
    psd.string_data_.insert({Fields::kPartialViewEnabled, kNotAvailable});
    psd.string_data_.insert({Fields::kUrlDownload, kNotAvailable});
    psd.string_data_.insert({Fields::kUrlReferrer, kNotAvailable});
    psd.string_data_.insert({Fields::kFilename, kNotAvailable});
    psd.string_data_.insert({Fields::kWarningInteractions, kNotAvailable});
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
  } else {
    psd.string_data_.insert({Fields::kUrlDownload, kNotLoggedNoSafeBrowsing});
    psd.string_data_.insert({Fields::kUrlReferrer, kNotLoggedNoSafeBrowsing});
    psd.string_data_.insert({Fields::kFilename, kNotLoggedNoSafeBrowsing});
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
  } else {
    psd.string_data_.insert(
        {Fields::kWarningInteractions, kNotLoggedNoEnhancedProtection});
  }

  return psd;
}

void DownloadWarningHatsProductSpecificData::AddNumPageWarnings(int num) {
  if (IsDownloadsPageTrigger(survey_type_)) {
    string_data_.insert_or_assign(Fields::kNumPageWarnings,
                                  base::NumberToString(num));
  }
}

void DownloadWarningHatsProductSpecificData::AddPartialViewInteraction(
    bool partial_view_interaction) {
  if (IsDownloadBubbleTrigger(survey_type_)) {
    bits_data_.insert_or_assign(Fields::kPartialViewInteraction,
                                partial_view_interaction);
  }
}

// static
std::vector<std::string>
DownloadWarningHatsProductSpecificData::GetBitsDataFields(
    DownloadWarningHatsType survey_type) {
  std::vector<std::string> fields = {Fields::kPartialViewEnabled,
                                     Fields::kUserGesture};
  if (IsDownloadBubbleTrigger(survey_type)) {
    fields.push_back(Fields::kPartialViewInteraction);
  }
  return fields;
}

// static
std::vector<std::string>
DownloadWarningHatsProductSpecificData::GetStringDataFields(
    DownloadWarningHatsType survey_type) {
  std::vector<std::string> fields = {
      Fields::kOutcome,
      Fields::kSurface,
      Fields::kDangerType,
      Fields::kWarningType,
      Fields::kSafeBrowsingState,
      Fields::kChannel,
      Fields::kWarningInteractions,
      Fields::kSecondsSinceDownloadStarted,
      Fields::kSecondsSinceWarningShown,
      Fields::kUrlDownload,
      Fields::kUrlReferrer,
      Fields::kFilename,
      // TODO(chlily): Add kIgnoreTimeout.
  };
  if (IsDownloadsPageTrigger(survey_type)) {
    fields.push_back(Fields::kNumPageWarnings);
  }
  if (survey_type == DownloadWarningHatsType::kDownloadBubbleIgnore) {
    fields.push_back(Fields::kIgnoreTimeoutSeconds);
  }
  return fields;
}

DelayedDownloadWarningHatsLauncher::Task::Task(
    DelayedDownloadWarningHatsLauncher& hats_launcher,
    download::DownloadItem* download,
    base::OnceClosure task,
    base::TimeDelta delay)
    : observation_(&hats_launcher), task_(std::move(task)) {
  observation_.Observe(download);
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(&Task::RunTask, weak_factory_.GetWeakPtr()),
      delay);
}

DelayedDownloadWarningHatsLauncher::Task::~Task() = default;

// It is expected that the caller will delete this after this executes.
void DelayedDownloadWarningHatsLauncher::Task::RunTask() {
  CHECK(task_);
  std::move(task_).Run();
}

DelayedDownloadWarningHatsLauncher::DelayedDownloadWarningHatsLauncher(
    Profile* profile,
    base::TimeDelta delay,
    PsdCompleter psd_completer)
    : profile_(profile),
      delay_(delay),
      psd_completer_(std::move(psd_completer)) {}

DelayedDownloadWarningHatsLauncher::~DelayedDownloadWarningHatsLauncher() =
    default;

void DelayedDownloadWarningHatsLauncher::OnDownloadUpdated(
    download::DownloadItem* download) {
  // If the formerly-eligible download is no longer eligible, cancel the survey.
  if (!CanShowDownloadWarningHatsSurvey(download)) {
    RemoveTaskIfAny(download);
  }
}

void DelayedDownloadWarningHatsLauncher::OnDownloadDestroyed(
    download::DownloadItem* download) {
  RemoveTaskIfAny(download);
}

void DelayedDownloadWarningHatsLauncher::RecordBrowserActivity() {
  last_activity_ = base::Time::Now();
}

bool DelayedDownloadWarningHatsLauncher::TryScheduleTask(
    DownloadWarningHatsType survey_type,
    download::DownloadItem* download) {
  CHECK(download);
  TaskKey key = GetTaskKey(download);
  if (base::Contains(tasks_, key)) {
    return false;
  }

  if (!CanShowDownloadWarningHatsSurvey(download)) {
    return false;
  }

  auto [_, inserted] = tasks_.try_emplace(
      key, *this, download,
      // Unretained is safe because `this` outlives the Task object.
      // `download` will be valid for as long as the Task lives, because the
      // Observer mechanism will delete the Task if `download` goes away.
      base::BindOnce(&DelayedDownloadWarningHatsLauncher::MaybeLaunchSurveyNow,
                     base::Unretained(this), survey_type, download),
      delay_);
  return inserted;
}

void DelayedDownloadWarningHatsLauncher::RemoveTaskIfAny(
    download::DownloadItem* download) {
  RemoveTaskByKeyIfAny(GetTaskKey(download));
}

DelayedDownloadWarningHatsLauncher::TaskKey
DelayedDownloadWarningHatsLauncher::GetTaskKey(
    download::DownloadItem* download) {
  return reinterpret_cast<TaskKey>(download);
}

void DelayedDownloadWarningHatsLauncher::RemoveTaskByKeyIfAny(TaskKey key) {
  tasks_.erase(key);
}

void DelayedDownloadWarningHatsLauncher::MaybeLaunchSurveyNow(
    DownloadWarningHatsType survey_type,
    download::DownloadItem* download) {
  if (!CanShowDownloadWarningHatsSurvey(download) || !WasUserActive()) {
    RemoveTaskIfAny(download);
    return;
  }

  auto psd =
      DownloadWarningHatsProductSpecificData::Create(survey_type, download);
  if (psd_completer_) {
    psd_completer_.Run(psd);
  }

  MaybeLaunchDownloadWarningHatsSurvey(profile_, psd,
                                       MakeSurveyDoneCallback(download),
                                       MakeSurveyDoneCallback(download));
}

base::OnceClosure DelayedDownloadWarningHatsLauncher::MakeSurveyDoneCallback(
    download::DownloadItem* download) {
  // This is needed to clean up the Task object after the survey runs. It must
  // be bound to a WeakPtr because nothing guarantees that this will be alive
  // when the survey task finishes (it generally takes a few seconds to actually
  // show the survey, and obviously takes much longer for the user to work
  // through the survey). If this callback runs, then `this` must still be
  // alive, which means the DownloadItem::Observer mechanism is maintaining the
  // invariant that any download with an entry in `tasks_` must be alive.
  // Therefore, the DownloadItem* will not be used after it is freed.
  return base::BindOnce(
      &DelayedDownloadWarningHatsLauncher::RemoveTaskByKeyIfAny,
      weak_factory_.GetWeakPtr(), GetTaskKey(download));
}

bool DelayedDownloadWarningHatsLauncher::WasUserActive() const {
  return base::Time::Now() - last_activity_ <= delay_;
}

bool CanShowDownloadWarningHatsSurvey(download::DownloadItem* download) {
  CHECK(download);
  return download->IsDangerous() && !download->IsDone();
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

base::TimeDelta GetIgnoreDownloadBubbleWarningDelay() {
  return base::Seconds(
      safe_browsing::kDownloadWarningSurveyIgnoreDelaySeconds.Get());
}

void MaybeLaunchDownloadWarningHatsSurvey(
    Profile* profile,
    const DownloadWarningHatsProductSpecificData& psd,
    base::OnceClosure success_callback,
    base::OnceClosure failure_callback) {
  std::optional<std::string> trigger =
      MaybeGetDownloadWarningHatsTrigger(psd.survey_type());
  if (!trigger) {
    return;
  }

  HatsService* hats_service =
      HatsServiceFactory::GetForProfile(profile, /*create_if_necessary=*/true);
  if (hats_service) {
    hats_service->LaunchSurvey(*trigger, std::move(success_callback),
                               std::move(failure_callback), psd.bits_data(),
                               psd.string_data());
  }
}
