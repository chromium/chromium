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

using Event = notice::mojom::PrivacySandboxNoticeEvent;
using enum Event;

constexpr int kCurrentSchemaVersion = 2;

// Notice data will be saved as a dictionary in the PrefService of a profile.

// PrefService path.
constexpr char kNoticeDataPath[] = "privacy_sandbox.notices";

// Unsynced pref that indicates the schema version this profile is using in
// regards to the data model.
constexpr char kSchemaVersionKey[] = "schema_version";

// Unsynced pref that indicates the chrome version this profile was initially
// shown the notice at. For migrated notices, this pref is empty.
constexpr char kChromeVersionKey[] = "chrome_version";

// Unsynced pref that indicates the events taken on the notice. Stored as a
// sorted list in order of event performed containing dict entries.
constexpr char kEventsKey[] = "events";

// Key value in the dict entry contained within `events`
constexpr char kEventKey[] = "event";

// Key value in the dict entry contained within `events`
constexpr char kTimestampKey[] = "timestamp";

// V1 Fields - DEPRECATED
constexpr char kNoticeActionTakenKey[] = "notice_action_taken";
constexpr char kNoticeActionTakenTimeKey[] = "notice_action_taken_time";
constexpr char kNoticeLastShownKey[] = "notice_last_shown";

template <typename T>
std::optional<T> ConvertTo(const base::DictValue* dict) {
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

NoticeActionTaken NoticeEventToNoticeAction(Event action) {
  switch (action) {
    case kAck:
      return NoticeActionTaken::kAck;
    case kClosed:
      return NoticeActionTaken::kClosed;
    case kOptIn:
      return NoticeActionTaken::kOptIn;
    case kOptOut:
      return NoticeActionTaken::kOptOut;
    case kSettings:
      return NoticeActionTaken::kSettings;
    default:
      return NoticeActionTaken::kNotSet;
  }
}

base::DictValue BuildDictEntryEvent(Event event, base::Time event_time) {
  return base::DictValue()
      .Set(kEventKey, static_cast<int>(event))
      .Set(kTimestampKey, base::TimeToValue(event_time));
}

base::DictValue BuildDictEntryEvent(NoticeEventTimestampPair* pair) {
  CHECK(pair);
  return BuildDictEntryEvent(pair->event, pair->timestamp);
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
                          const NoticeStorageData& data) {
  ScopedDictPrefUpdate update(pref_service, kNoticeDataPath);
  base::DictValue* dict = update->EnsureDict(notice);
  dict->Set(kSchemaVersionKey, data.schema_version);
  if (data.notice_events.empty()) {
    return;
  }
  base::ListValue* event_list = dict->EnsureList(kEventsKey);
  for (const auto& event_ptr : data.notice_events) {
    event_list->Append(BuildDictEntryEvent(event_ptr.get()));
  }
}

void MaybeEraseV1Fields(PrefService* pref_service, std::string_view notice) {
  ScopedDictPrefUpdate update(pref_service, kNoticeDataPath);
  base::DictValue* dict = update->FindDict(notice);
  CHECK(dict);
  for (const char* key : {kNoticeActionTakenKey, kNoticeActionTakenTimeKey,
                          kNoticeLastShownKey}) {
    dict->Remove(key);
  }
}

}  // namespace

std::optional<base::Time> GetNoticeFirstShownFromEvents(
    const NoticeStorageData& notice_data) {
  for (const auto& notice_event : notice_data.notice_events) {
    if (notice_event->event == kShown) {
      return notice_event->timestamp;
    }
  }
  return std::nullopt;
}

std::optional<base::Time> GetNoticeLastShownFromEvents(
    const NoticeStorageData& notice_data) {
  for (const auto& notice_event : base::Reversed(notice_data.notice_events)) {
    if (notice_event->event == kShown) {
      return notice_event->timestamp;
    }
  }
  return std::nullopt;
}

std::optional<NoticeEventTimestampPair>
GetNoticeActionTakenForFirstShownFromEvents(
    const NoticeStorageData& notice_data) {
  std::optional<NoticeEventTimestampPair> notice_event_data;
  int last_shown_idx = 0;
  int first_notice_idx = 0;
  for (const auto& event_data : notice_data.notice_events) {
    if (event_data->event == kShown) {
      last_shown_idx++;
    } else if (!notice_event_data.has_value() ||
               first_notice_idx == last_shown_idx) {
      first_notice_idx = last_shown_idx;
      notice_event_data = *event_data;
    }
  }
  return notice_event_data;
}

void NoticeEventTimestampPair::RegisterJSONConverter(
    base::JSONValueConverter<NoticeEventTimestampPair>* converter) {
  converter->RegisterCustomValueField<Event>(
      kEventKey, &NoticeEventTimestampPair::event, &MaybeValueToEnum<Event>);
  converter->RegisterCustomValueField<base::Time>(
      kTimestampKey, &NoticeEventTimestampPair::timestamp, &MaybeValueToTime);
}

// PrivacySandboxNoticeData definitions.
NoticeStorageData::NoticeStorageData() = default;
NoticeStorageData::~NoticeStorageData() = default;

NoticeStorageData::NoticeStorageData(NoticeStorageData&& data) = default;
NoticeStorageData& NoticeStorageData::operator=(NoticeStorageData&& data) =
    default;

bool NoticeStorageData::operator==(const NoticeStorageData& other) const {
  if (schema_version != other.schema_version ||
      chrome_version != other.chrome_version ||
      notice_events.size() != other.notice_events.size()) {
    return false;
  }
  for (size_t i = 0; i < notice_events.size(); ++i) {
    const auto& lhs_event_ptr = notice_events[i];
    const auto& rhs_event_ptr = other.notice_events[i];
    if (!lhs_event_ptr && !rhs_event_ptr) {
      continue;
    }
    if (!lhs_event_ptr || !rhs_event_ptr) {
      return false;
    }
    if (!(*lhs_event_ptr == *rhs_event_ptr)) {
      return false;
    }
  }
  return true;
}

void NoticeStorageData::RegisterJSONConverter(
    base::JSONValueConverter<NoticeStorageData>* converter) {
  converter->RegisterIntField(kSchemaVersionKey,
                              &NoticeStorageData::schema_version);
  converter->RegisterStringField(kChromeVersionKey,
                                 &NoticeStorageData::chrome_version);
  converter->RegisterRepeatedMessage<NoticeEventTimestampPair>(
      kEventsKey, &NoticeStorageData::notice_events);
}

void V1MigrationData::RegisterJSONConverter(
    base::JSONValueConverter<V1MigrationData>* converter) {
  converter->RegisterIntField(kSchemaVersionKey,
                              &V1MigrationData::schema_version);
  converter->RegisterCustomValueField<NoticeActionTaken>(
      kNoticeActionTakenKey, &V1MigrationData::notice_action_taken,
      &MaybeValueToEnum<NoticeActionTaken>);
  converter->RegisterCustomValueField<base::Time>(
      kNoticeActionTakenTimeKey, &V1MigrationData::notice_action_taken_time,
      &MaybeValueToTime);
  converter->RegisterCustomValueField<base::Time>(
      kNoticeLastShownKey, &V1MigrationData::notice_last_shown,
      &MaybeValueToTime);
}

// PrivacySandboxNoticeStorage definitions.
void PrivacySandboxNoticeStorage::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kNoticeDataPath);
}

// static
std::string PrivacySandboxNoticeStorage::GetNoticeActionStringFromEvent(
    Event event) {
  switch (event) {
    case kShown:
      return "";
    case kAck:
      return "Ack";
    case kClosed:
      return "Closed";
    case kOptIn:
      return "OptIn";
    case kOptOut:
      return "OptOut";
    case kSettings:
      return "Settings";
  }
}

// static
std::optional<Event> PrivacySandboxNoticeStorage::NoticeActionToNoticeEvent(
    NoticeActionTaken action) {
  switch (action) {
    case NoticeActionTaken::kAck:
      return kAck;
    case NoticeActionTaken::kClosed:
      return kClosed;
    case NoticeActionTaken::kOptIn:
      return kOptIn;
    case NoticeActionTaken::kOptOut:
      return kOptOut;
    case NoticeActionTaken::kSettings:
      return kSettings;
    default:
      return std::nullopt;
  }
}

// static
NoticeStorageData PrivacySandboxNoticeStorage::ToV2Schema(
    const V1MigrationData& data_v1) {
  std::vector<std::unique_ptr<NoticeEventTimestampPair>> notice_events;

  if (data_v1.notice_last_shown != base::Time()) {
    notice_events.emplace_back(std::make_unique<NoticeEventTimestampPair>(
        NoticeEventTimestampPair{kShown, data_v1.notice_last_shown}));
  }

  auto notice_event = NoticeActionToNoticeEvent(data_v1.notice_action_taken);
  if (notice_event.has_value()) {
    notice_events.emplace_back(
        std::make_unique<NoticeEventTimestampPair>(NoticeEventTimestampPair{
            *notice_event, data_v1.notice_action_taken_time}));
  }

  NoticeStorageData notice_data;
  notice_data.schema_version = 2;
  notice_data.notice_events = std::move(notice_events);
  return notice_data;
}

// static
void PrivacySandboxNoticeStorage::UpdateNoticeSchemaV2(
    PrefService* pref_service) {
  if (!base::FeatureList::IsEnabled(kPrivacySandboxMigratePrefsToSchemaV2)) {
    return;
  }
  const auto* notice_data_pref =
      pref_service->GetUserPrefValue(kNoticeDataPath);
  if (!notice_data_pref) {
    return;
  }

  const base::DictValue* data = notice_data_pref->GetIfDict();

  if (!data) {
    return;
  }

  for (const auto [notice, notice_value] : *data) {
    auto data_v1 = ConvertTo<V1MigrationData>(&notice_value);
    if (!data_v1) {
      continue;
    }

    if (data_v1->schema_version == 1) {
      PopulateV2NoticeData(pref_service, notice, ToV2Schema(*data_v1));
    }

    // We always erase V1 fields. Even if the current version isn't V1. This is
    // because the previously migration to V2 didn't erase the V1 fields.
    MaybeEraseV1Fields(pref_service, notice);
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
       pref_service_->GetDict(kNoticeDataPath)) {
    auto notice_data = ConvertTo<NoticeStorageData>(&notice_value);

    NoticeStartupState startup_state;

    if (!notice_data.has_value() || notice_data->notice_events.empty() ||
        (GetNoticeFirstShownFromEvents(*notice_data) == std::nullopt &&
         GetNoticeActionTakenForFirstShownFromEvents(*notice_data) ==
             std::nullopt)) {
      startup_state = NoticeStartupState::kPromptNotShown;
    } else if (auto time = GetNoticeFirstShownFromEvents(*notice_data);
               time == std::nullopt || time == base::Time()) {
      // E.g. UnknownActionPreMigration && no first shown time set.
      startup_state = NoticeStartupState::kUnknownState;
    } else {  // Notice has been shown, action handling below.
      switch (notice_data->notice_events.back().get()->event) {
        case kShown:
          startup_state = NoticeStartupState::kPromptWaiting;
          break;
        case kOptIn:
          startup_state = NoticeStartupState::kFlowCompletedWithOptIn;
          break;
        case kOptOut:
          startup_state = NoticeStartupState::kFlowCompletedWithOptOut;
          break;
        case kAck:
        case kClosed:
        case kSettings:
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

std::optional<NoticeStorageData> PrivacySandboxNoticeStorage::ReadNoticeData(
    std::string_view notice) const {
  const base::DictValue& pref_data = pref_service_->GetDict(kNoticeDataPath);
  return ConvertTo<NoticeStorageData>(pref_data.FindDict(notice));
}

void PrivacySandboxNoticeStorage::RecordEvent(NoticeId notice_id, Event event) {
  const Notice& notice = FindNotice(notice_id, catalog_);

  if (event == kShown) {
    SetNoticeShown(notice.GetStorageName(), base::Time::Now());
    return;
  }
  SetNoticeActionTaken(notice.GetStorageName(), event, base::Time::Now());
}

void PrivacySandboxNoticeStorage::SetNoticeActionTaken(
    std::string_view notice,
    Event notice_action_taken,
    base::Time notice_action_taken_time) {
  CHECK(notice_action_taken != kShown)
      << "Use `SetNoticeShown` to set a kShown PrivacySandboxNoticeEvent "
         "instead.";
  auto notice_data = ReadNoticeData(notice);

  // The notice should be shown first before action can be taken on it.
  if (!notice_data.has_value() ||
      GetNoticeLastShownFromEvents(*notice_data) == std::nullopt) {
    base::UmaHistogramEnumeration(
        base::StrCat(
            {"PrivacySandbox.Notice.NoticeActionTakenBehavior.", notice}),
        NoticeActionBehavior::kActionBeforeShown);
    return;
  }

  // Performing multiple actions on an existing notice is unexpected.
  if (notice_data->notice_events.back().get()->event != kShown) {
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

  ScopedDictPrefUpdate update(pref_service_, kNoticeDataPath);
  update->EnsureDict(notice)
      ->EnsureList(kEventsKey)
      ->Append(
          BuildDictEntryEvent(notice_action_taken, notice_action_taken_time));

  base::UmaHistogramEnumeration(
      base::StrCat(
          {"PrivacySandbox.Notice.NoticeActionTakenBehavior.", notice}),
      NoticeActionBehavior::kSuccess);

  std::string notice_action_str =
      PrivacySandboxNoticeStorage::GetNoticeActionStringFromEvent(
          notice_action_taken);
  if (!notice_action_str.empty()) {
    // First shown to interacted duration.
    auto notice_first_shown = GetNoticeFirstShownFromEvents(*notice_data);
    if (notice_first_shown) {
      base::TimeDelta first_shown_to_interacted_duration =
          notice_action_taken_time - *notice_first_shown;
      CreateTimingHistogram(
          base::StrCat({"PrivacySandbox.Notice.FirstShownToInteractedDuration.",
                        notice, "_", notice_action_str}),
          first_shown_to_interacted_duration);
    }

    // Set last shown to interacted.
    auto notice_last_shown = GetNoticeLastShownFromEvents(*notice_data);
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
  ScopedDictPrefUpdate update(pref_service_, kNoticeDataPath);
  base::DictValue* dict = update->EnsureDict(notice);
  dict->Set(kSchemaVersionKey, kCurrentSchemaVersion);
  dict->Set(kChromeVersionKey, version_info::GetVersionNumber());
  dict->EnsureList(kEventsKey)
      ->Append(BuildDictEntryEvent(kShown, notice_shown_time));

  // TODO(chrstne): Deprecate NoticeShown histogram once it is
  // no longer used in other codepaths.
  base::UmaHistogramBoolean(
      base::StrCat({"PrivacySandbox.Notice.NoticeShown.", notice}), true);
  base::UmaHistogramEnumeration(
      base::StrCat({"PrivacySandbox.Notice.NoticeEvent.", notice}), kShown);

  auto notice_data = ReadNoticeData(notice);
  CHECK(notice_data.has_value());
  if (GetNoticeFirstShownFromEvents(*notice_data) == notice_shown_time) {
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
