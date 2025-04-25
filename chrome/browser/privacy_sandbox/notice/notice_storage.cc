// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/notice/notice_storage.h"

#include <string>

#include "base/containers/adapters.h"
#include "base/feature_list.h"
#include "base/json/values_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/version_info/version_info.h"
#include "chrome/browser/privacy_sandbox/notice/notice_catalog.h"
#include "chrome/browser/privacy_sandbox/notice/notice_model.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"

namespace privacy_sandbox {
namespace {
using notice::mojom::PrivacySandboxNotice;
using notice::mojom::PrivacySandboxNoticeEvent;

// Notice data will be saved as a dictionary in the PrefService of a profile.

// PrefService path.
constexpr char kPrivacySandboxNoticeDataPath[] = "privacy_sandbox.notices";

// Unsynced pref that indicates the schema version this profile is using in
// regards to the data model.
constexpr char kPrivacySandboxSchemaVersion[] = "schema_version";

// Unsynced pref that indicates the chrome version this profile was initially
// shown the notice at. For migrated notices, this pref is empty.
constexpr char kPrivacySandboxChromeVersion[] = "chrome_version";

// Unsynced pref that indicates the events taken on the notice. Stored as a
// sorted list in order of event performed containing dict entries.
constexpr char kPrivacySandboxEvents[] = "events";

// Deprecated. Do not use for new values.
constexpr char kPrivacySandboxNoticeActionTaken[] = "notice_action_taken";

// Deprecated. Do not use for new values.
constexpr char kPrivacySandboxNoticeActionTakenTime[] =
    "notice_action_taken_time";

// Deprecated. Do not use for new values.
constexpr char kPrivacySandboxNoticeLastShown[] = "notice_last_shown";

// Key value in the dict entry contained within `events`
constexpr char kPrivacySandboxNoticeEvent[] = "event";

// Key value in the dict entry contained within `events`
constexpr char kPrivacySandboxNoticeEventTime[] = "timestamp";

constexpr int kPrivacySandboxNoticeSchemaVersion = 2;

std::string CreatePrefPath(std::string_view notice,
                           std::string_view pref_name) {
  return base::StrCat({notice, ".", pref_name});
}

template <typename T>
std::optional<T> ConvertTo(const base::Value::Dict* dict) {
  if (!dict) {
    return std::nullopt;
  }
  base::JSONValueConverter<T> converter;
  T data;
  if (converter.Convert(*dict, &data)) {
    return data;
  }
  return std::nullopt;
}

template <typename T>
std::optional<T> ConvertTo(const base::Value* value) {
  return value ? ConvertTo<T>(value->GetIfDict()) : std::nullopt;
}

void CreateTimingHistogram(const std::string& name, base::TimeDelta sample) {
  base::UmaHistogramCustomTimes(name, sample, base::Milliseconds(1),
                                base::Days(10), 100);
}

NoticeActionTaken NoticeEventToNoticeAction(PrivacySandboxNoticeEvent action) {
  switch (action) {
    case PrivacySandboxNoticeEvent::kAck:
      return NoticeActionTaken::kAck;
    case PrivacySandboxNoticeEvent::kClosed:
      return NoticeActionTaken::kClosed;
    case PrivacySandboxNoticeEvent::kOptIn:
      return NoticeActionTaken::kOptIn;
    case PrivacySandboxNoticeEvent::kOptOut:
      return NoticeActionTaken::kOptOut;
    case PrivacySandboxNoticeEvent::kSettings:
      return NoticeActionTaken::kSettings;
    default:
      return NoticeActionTaken::kNotSet;
  }
}

void SetSchemaVersion(PrefService* pref_service, std::string_view notice) {
  ScopedDictPrefUpdate update(pref_service, kPrivacySandboxNoticeDataPath);
  update.Get().SetByDottedPath(
      CreatePrefPath(notice, kPrivacySandboxSchemaVersion),
      kPrivacySandboxNoticeSchemaVersion);
}

base::Value::Dict BuildDictEntryEvent(PrivacySandboxNoticeEvent event,
                                      base::Time event_time) {
  base::Value::Dict params;
  params.Set(kPrivacySandboxNoticeEvent, static_cast<int>(event));
  params.Set(kPrivacySandboxNoticeEventTime, base::TimeToValue(event_time));
  return params;
}

void SetChromeVersion(PrefService* pref_service, std::string_view notice) {
  ScopedDictPrefUpdate update(pref_service, kPrivacySandboxNoticeDataPath);
  update.Get().SetByDottedPath(
      CreatePrefPath(notice, kPrivacySandboxChromeVersion),
      version_info::GetVersionNumber());
}

const Notice& FindNotice(NoticeId notice_id, NoticeCatalog* catalog) {
  const auto& notice_map = catalog->GetNoticeMap();
  auto notice_ptr = catalog->GetNoticeMap().find(notice_id);
  CHECK(notice_ptr != notice_map.end());
  CHECK(notice_ptr->second != nullptr);
  return *(notice_ptr->second.get());
}

bool MaybeValueToTime(const base::Value* value, base::Time* time) {
  if (!value || !value->is_string()) {
    return false;
  }

  std::optional<base::Time> parsed_time = base::ValueToTime(value);
  if (!parsed_time) {
    return false;
  }
  *time = *parsed_time;
  return true;
}

template <typename T>
bool MaybeValueToEnum(const base::Value* value, T* output) {
  if (!value || !value->is_int()) {
    return false;
  }
  int int_value = value->GetInt();
  if (int_value < static_cast<int>(T::kMinValue) ||
      int_value > static_cast<int>(T::kMaxValue)) {
    return false;
  }
  *output = static_cast<T>(int_value);
  return true;
}

void PopulateV2NoticeData(PrefService* pref_service,
                          std::string_view notice,
                          const PrivacySandboxNoticeData& data) {
  ScopedDictPrefUpdate update(pref_service, kPrivacySandboxNoticeDataPath);
  update.Get().SetByDottedPath(
      CreatePrefPath(notice, kPrivacySandboxSchemaVersion),
      data.GetSchemaVersion());

  for (const auto& event : data.GetNoticeEvents()) {
    update.Get()
        .EnsureDict(notice)
        ->EnsureList(kPrivacySandboxEvents)
        ->Append(
            BuildDictEntryEvent(event.get()->event, event.get()->timestamp));
  }
}

}  // namespace

void NoticeEventTimestampPair::RegisterJSONConverter(
    base::JSONValueConverter<NoticeEventTimestampPair>* converter) {
  converter->RegisterCustomValueField<PrivacySandboxNoticeEvent>(
      kPrivacySandboxNoticeEvent, &NoticeEventTimestampPair::event,
      &MaybeValueToEnum<PrivacySandboxNoticeEvent>);
  converter->RegisterCustomValueField<base::Time>(
      kPrivacySandboxNoticeEventTime, &NoticeEventTimestampPair::timestamp,
      &MaybeValueToTime);
}

// PrivacySandboxNoticeData definitions.
PrivacySandboxNoticeData::PrivacySandboxNoticeData() = default;

PrivacySandboxNoticeData::~PrivacySandboxNoticeData() = default;

PrivacySandboxNoticeData::PrivacySandboxNoticeData(
    PrivacySandboxNoticeData&& data) = default;
PrivacySandboxNoticeData& PrivacySandboxNoticeData::operator=(
    PrivacySandboxNoticeData&& data) = default;

int PrivacySandboxNoticeData::GetSchemaVersion() const {
  return schema_version_;
}
std::string PrivacySandboxNoticeData::GetChromeVersion() const {
  return chrome_version_;
}
base::span<const std::unique_ptr<NoticeEventTimestampPair>>
PrivacySandboxNoticeData::GetNoticeEvents() const {
  return notice_events_;
}

void PrivacySandboxNoticeData::SetSchemaVersion(int schema_version) {
  schema_version_ = schema_version;
}

void PrivacySandboxNoticeData::SetChromeVersion(
    std::string_view chrome_version) {
  chrome_version_ = chrome_version;
}

void PrivacySandboxNoticeData::SetNoticeEvents(
    std::vector<std::unique_ptr<NoticeEventTimestampPair>>&& events) {
  notice_events_ = std::move(events);
}

std::optional<base::Time>
PrivacySandboxNoticeData::GetNoticeFirstShownFromEvents() const {
  for (const auto& notice_event : notice_events_) {
    if (notice_event->event == PrivacySandboxNoticeEvent::kShown) {
      return notice_event->timestamp;
    }
  }
  return std::nullopt;
}

std::optional<base::Time>
PrivacySandboxNoticeData::GetNoticeLastShownFromEvents() const {
  for (const auto& notice_event : base::Reversed(notice_events_)) {
    if (notice_event->event == PrivacySandboxNoticeEvent::kShown) {
      return notice_event->timestamp;
    }
  }
  return std::nullopt;
}

std::optional<NoticeEventTimestampPair>
PrivacySandboxNoticeData::GetNoticeActionTakenForFirstShownFromEvents() const {
  std::optional<NoticeEventTimestampPair> notice_event_data;
  int last_shown_idx = 0;
  int first_notice_idx = 0;
  for (const auto& event_data : notice_events_) {
    if (event_data->event == PrivacySandboxNoticeEvent::kShown) {
      last_shown_idx++;
    } else if (!notice_event_data.has_value() ||
               first_notice_idx == last_shown_idx) {
      first_notice_idx = last_shown_idx;
      notice_event_data = *event_data;
    }
  }
  return notice_event_data;
}

void PrivacySandboxNoticeData::RegisterJSONConverter(
    base::JSONValueConverter<PrivacySandboxNoticeData>* converter) {
  converter->RegisterIntField(kPrivacySandboxSchemaVersion,
                              &PrivacySandboxNoticeData::schema_version_);
  converter->RegisterStringField(kPrivacySandboxChromeVersion,
                                 &PrivacySandboxNoticeData::chrome_version_);
  converter->RegisterRepeatedMessage<NoticeEventTimestampPair>(
      kPrivacySandboxEvents, &PrivacySandboxNoticeData::notice_events_);
}

void V1MigrationData::RegisterJSONConverter(
    base::JSONValueConverter<V1MigrationData>* converter) {
  converter->RegisterIntField(kPrivacySandboxSchemaVersion,
                              &V1MigrationData::schema_version);
  converter->RegisterCustomValueField<NoticeActionTaken>(
      kPrivacySandboxNoticeActionTaken, &V1MigrationData::notice_action_taken,
      &MaybeValueToEnum<NoticeActionTaken>);
  converter->RegisterCustomValueField<base::Time>(
      kPrivacySandboxNoticeActionTakenTime,
      &V1MigrationData::notice_action_taken_time, &MaybeValueToTime);
  converter->RegisterCustomValueField<base::Time>(
      kPrivacySandboxNoticeLastShown, &V1MigrationData::notice_last_shown,
      &MaybeValueToTime);
}

// PrivacySandboxNoticeStorage definitions.
void PrivacySandboxNoticeStorage::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kPrivacySandboxNoticeDataPath);
}

// static
std::string PrivacySandboxNoticeStorage::GetNoticeActionStringFromEvent(
    PrivacySandboxNoticeEvent event) {
  switch (event) {
    case PrivacySandboxNoticeEvent::kShown:
      return "";
    case PrivacySandboxNoticeEvent::kAck:
      return "Ack";
    case PrivacySandboxNoticeEvent::kClosed:
      return "Closed";
    case PrivacySandboxNoticeEvent::kOptIn:
      return "OptIn";
    case PrivacySandboxNoticeEvent::kOptOut:
      return "OptOut";
    case PrivacySandboxNoticeEvent::kSettings:
      return "Settings";
  }
}

// static
std::optional<PrivacySandboxNoticeEvent>
PrivacySandboxNoticeStorage::NoticeActionToNoticeEvent(
    NoticeActionTaken action) {
  switch (action) {
    case NoticeActionTaken::kAck:
      return PrivacySandboxNoticeEvent::kAck;
    case NoticeActionTaken::kClosed:
      return PrivacySandboxNoticeEvent::kClosed;
    case NoticeActionTaken::kOptIn:
      return PrivacySandboxNoticeEvent::kOptIn;
    case NoticeActionTaken::kOptOut:
      return PrivacySandboxNoticeEvent::kOptOut;
    case NoticeActionTaken::kSettings:
      return PrivacySandboxNoticeEvent::kSettings;
    default:
      return std::nullopt;
  }
}

// static
PrivacySandboxNoticeData PrivacySandboxNoticeStorage::ToV2Schema(
    const V1MigrationData& data_v1) {
  PrivacySandboxNoticeData data_v2;
  std::vector<std::unique_ptr<NoticeEventTimestampPair>> notice_events;
  data_v2.SetSchemaVersion(2);

  if (data_v1.notice_last_shown != base::Time()) {
    notice_events.emplace_back(
        std::make_unique<NoticeEventTimestampPair>(NoticeEventTimestampPair{
            PrivacySandboxNoticeEvent::kShown, data_v1.notice_last_shown}));
  }

  auto notice_event = NoticeActionToNoticeEvent(data_v1.notice_action_taken);
  if (notice_event.has_value()) {
    notice_events.emplace_back(
        std::make_unique<NoticeEventTimestampPair>(NoticeEventTimestampPair{
            *notice_event, data_v1.notice_action_taken_time}));
  }

  data_v2.SetNoticeEvents(std::move(notice_events));
  return data_v2;
}

// static
void PrivacySandboxNoticeStorage::UpdateNoticeSchemaV2(
    PrefService* pref_service) {
  if (!base::FeatureList::IsEnabled(kPrivacySandboxMigratePrefsToSchemaV2)) {
    return;
  }
  const auto* notice_data_pref =
      pref_service->GetUserPrefValue(kPrivacySandboxNoticeDataPath);
  if (!notice_data_pref) {
    return;
  }

  const base::Value::Dict* data = notice_data_pref->GetIfDict();

  if (!data) {
    return;
  }

  for (const auto [notice, notice_value] : *data) {
    auto data_v1 = ConvertTo<V1MigrationData>(&notice_value);
    if (!data_v1 || data_v1->schema_version != 1) {
      continue;
    }
    PopulateV2NoticeData(pref_service, notice, ToV2Schema(*data_v1));

    // TODO(boujane) Erase V1 Only fields.
  }
}

NoticeStorage::~NoticeStorage() = default;

PrivacySandboxNoticeStorage::PrivacySandboxNoticeStorage(
    PrefService* pref_service,
    NoticeCatalog* catalog)
    : pref_service_(pref_service), catalog_(catalog) {
  CHECK(pref_service_);
  CHECK(catalog_);
}

PrivacySandboxNoticeStorage::~PrivacySandboxNoticeStorage() = default;

void PrivacySandboxNoticeStorage::RecordStartupHistograms() const {
  for (const auto [notice, notice_value] :
       pref_service_->GetDict(kPrivacySandboxNoticeDataPath)) {
    auto notice_data = ConvertTo<PrivacySandboxNoticeData>(&notice_value);

    NoticeStartupState startup_state;

    if (!notice_data.has_value() || notice_data->GetNoticeEvents().empty() ||
        (notice_data->GetNoticeFirstShownFromEvents() == std::nullopt &&
         notice_data->GetNoticeActionTakenForFirstShownFromEvents() ==
             std::nullopt)) {
      startup_state = NoticeStartupState::kPromptNotShown;
    } else if (notice_data->GetNoticeFirstShownFromEvents() == std::nullopt ||
               *notice_data->GetNoticeFirstShownFromEvents() == base::Time()) {
      // E.g. UnknownActionPreMigration && no first shown time set.
      startup_state = NoticeStartupState::kUnknownState;
    } else {  // Notice has been shown, action handling below.
      switch (notice_data->GetNoticeEvents().back().get()->event) {
        case PrivacySandboxNoticeEvent::kShown:
          startup_state = NoticeStartupState::kPromptWaiting;
          break;
        case PrivacySandboxNoticeEvent::kOptIn:
          startup_state = NoticeStartupState::kFlowCompletedWithOptIn;
          break;
        case PrivacySandboxNoticeEvent::kOptOut:
          startup_state = NoticeStartupState::kFlowCompletedWithOptOut;
          break;
        case PrivacySandboxNoticeEvent::kAck:
        case PrivacySandboxNoticeEvent::kClosed:
        case PrivacySandboxNoticeEvent::kSettings:
          startup_state = NoticeStartupState::kFlowCompleted;
          break;
      }
    }
    base::UmaHistogramEnumeration(
        base::StrCat({"PrivacySandbox.Notice.NoticeStartupState2.", notice}),
        startup_state);
    // TODO(chrstne): Deprecate existing histogram.
    base::UmaHistogramEnumeration(
        base::StrCat({"PrivacySandbox.Notice.NoticeStartupState.", notice}),
        startup_state);
  }
}

std::optional<PrivacySandboxNoticeData>
PrivacySandboxNoticeStorage::ReadNoticeData(std::string_view notice) const {
  const base::Value::Dict& pref_data =
      pref_service_->GetDict(kPrivacySandboxNoticeDataPath);
  return ConvertTo<PrivacySandboxNoticeData>(pref_data.FindDict(notice));
}

void PrivacySandboxNoticeStorage::RecordEvent(
    NoticeId notice_id,
    notice::mojom::PrivacySandboxNoticeEvent event) {
  const Notice& notice = FindNotice(notice_id, catalog_);

  if (event == PrivacySandboxNoticeEvent::kShown) {
    SetNoticeShown(notice.GetStorageName(), base::Time::Now());
    return;
  }
  SetNoticeActionTaken(notice.GetStorageName(), event, base::Time::Now());
}

void PrivacySandboxNoticeStorage::SetNoticeActionTaken(
    std::string_view notice,
    PrivacySandboxNoticeEvent notice_action_taken,
    base::Time notice_action_taken_time) {
  CHECK(notice_action_taken != PrivacySandboxNoticeEvent::kShown)
      << "Use `SetNoticeShown` to set a kShown PrivacySandboxNoticeEvent "
         "instead.";
  ScopedDictPrefUpdate update(pref_service_, kPrivacySandboxNoticeDataPath);
  auto notice_data = ReadNoticeData(notice);

  // The notice should be shown first before action can be taken on it.
  if (!notice_data.has_value() ||
      notice_data->GetNoticeLastShownFromEvents() == std::nullopt) {
    base::UmaHistogramEnumeration(
        base::StrCat(
            {"PrivacySandbox.Notice.NoticeActionTakenBehavior.", notice}),
        NoticeActionBehavior::kActionBeforeShown);
    return;
  }

  // Performing multiple actions on an existing notice is unexpected.
  if (notice_data->GetNoticeEvents().back().get()->event !=
      PrivacySandboxNoticeEvent::kShown) {
    base::UmaHistogramEnumeration(
        base::StrCat(
            {"PrivacySandbox.Notice.NoticeActionTakenBehavior.", notice}),
        NoticeActionBehavior::kDuplicateActionTaken);
    return;
  }

  // Emitting histograms.
  // TODO(chrstne): Deprecate NoticeAction histogram once it is no longer used
  // in other codepaths.
  base::UmaHistogramEnumeration(
      base::StrCat({"PrivacySandbox.Notice.NoticeAction.", notice}),
      NoticeEventToNoticeAction(notice_action_taken));
  base::UmaHistogramEnumeration(
      base::StrCat({"PrivacySandbox.Notice.NoticeEvent.", notice}),
      notice_action_taken);

  base::Value::Dict entry =
      BuildDictEntryEvent(notice_action_taken, notice_action_taken_time);
  update.Get()
      .EnsureDict(notice)
      ->EnsureList(kPrivacySandboxEvents)
      ->Append(std::move(entry));

  base::UmaHistogramEnumeration(
      base::StrCat(
          {"PrivacySandbox.Notice.NoticeActionTakenBehavior.", notice}),
      NoticeActionBehavior::kSuccess);

  std::string notice_action_str =
      PrivacySandboxNoticeStorage::GetNoticeActionStringFromEvent(
          notice_action_taken);
  if (!notice_action_str.empty()) {
    // First shown to interacted duration.
    auto notice_first_shown = notice_data->GetNoticeFirstShownFromEvents();
    if (notice_first_shown) {
      base::TimeDelta first_shown_to_interacted_duration =
          notice_action_taken_time - *notice_first_shown;
      CreateTimingHistogram(
          base::StrCat({"PrivacySandbox.Notice.FirstShownToInteractedDuration.",
                        notice, "_", notice_action_str}),
          first_shown_to_interacted_duration);
    }

    // Set last shown to interacted.
    auto notice_last_shown = notice_data->GetNoticeLastShownFromEvents();
    if (notice_last_shown) {
      auto last_shown_to_interacted_duration =
          notice_action_taken_time - *notice_last_shown;
      CreateTimingHistogram(
          base::StrCat({"PrivacySandbox.Notice.LastShownToInteractedDuration.",
                        notice, "_", notice_action_str}),
          last_shown_to_interacted_duration);
    }
  }
}
void PrivacySandboxNoticeStorage::SetNoticeShown(std::string_view notice,
                                                 base::Time notice_shown_time) {
  ScopedDictPrefUpdate update(pref_service_, kPrivacySandboxNoticeDataPath);
  SetSchemaVersion(pref_service_, notice);
  SetChromeVersion(pref_service_, notice);

  base::Value::Dict entry =
      BuildDictEntryEvent(PrivacySandboxNoticeEvent::kShown, notice_shown_time);
  update.Get()
      .EnsureDict(notice)
      ->EnsureList(kPrivacySandboxEvents)
      ->Append(std::move(entry));

  // TODO(chrstne): Deprecate NoticeShown histogram once it is no longer used
  // in other codepaths.
  base::UmaHistogramBoolean(
      base::StrCat({"PrivacySandbox.Notice.NoticeShown.", notice}), true);
  base::UmaHistogramEnumeration(
      base::StrCat({"PrivacySandbox.Notice.NoticeEvent.", notice}),
      PrivacySandboxNoticeEvent::kShown);

  auto notice_data = ReadNoticeData(notice);
  if (*notice_data->GetNoticeFirstShownFromEvents() == notice_shown_time) {
    base::UmaHistogramBoolean(
        base::StrCat(
            {"PrivacySandbox.Notice.NoticeShownForFirstTime.", notice}),
        true);
  } else {
    base::UmaHistogramBoolean(
        base::StrCat(
            {"PrivacySandbox.Notice.NoticeShownForFirstTime.", notice}),
        false);
  }
}

}  // namespace privacy_sandbox
