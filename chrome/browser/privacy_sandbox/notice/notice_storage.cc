// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/notice/notice_storage.h"

#include <string>

#include "base/feature_list.h"
#include "base/json/values_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/version_info/version_info.h"
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

// --- Histogramming helpers ---
constexpr char kHistogramPrefix[] = "PrivacySandbox.Notice.";

template <typename T>
void RecordEnum(std::string_view category,
                std::string_view notice_id,
                T sample) {
  base::UmaHistogramEnumeration(
      base::StrCat({kHistogramPrefix, category, ".", notice_id}), sample);
}

void RecordBool(std::string_view category,
                std::string_view notice_id,
                bool sample) {
  base::UmaHistogramBoolean(
      base::StrCat({kHistogramPrefix, category, ".", notice_id}), sample);
}

void RecordTimingWithAction(std::string_view category,
                            std::string_view notice_id,
                            std::string_view suffix,
                            base::TimeDelta sample) {
  base::UmaHistogramCustomTimes(
      base::StrCat({kHistogramPrefix, category, ".", notice_id, "_", suffix}),
      sample, base::Milliseconds(1), base::Days(10), 100);
}

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

base::DictValue BuildDictEntryEvent(Event event,
                                    base::Time event_time = base::Time::Now()) {
  return base::DictValue()
      .Set(kEventKey, static_cast<int>(event))
      .Set(kTimestampKey, base::TimeToValue(event_time));
}

base::DictValue BuildDictEntryEvent(NoticeEventTimestampPair* pair) {
  CHECK(pair);
  return BuildDictEntryEvent(pair->event, pair->timestamp);
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

// Emits histograms for new events, comparing against existing notice_data for
// certain histograms.
void EmitNewEventHistograms(
    std::optional<NoticeStorageData> existing_notice_data,
    std::string_view notice,
    Event new_event) {
  // Emit NoticeEvent for all Events.
  RecordEnum("NoticeEvent", notice, new_event);
  // Deprecated histograms.
  new_event == kShown ? RecordBool("NoticeShown", notice, true)
                      : RecordEnum("NoticeAction", notice,
                                   NoticeEventToNoticeAction(new_event));

  if (!existing_notice_data) {
    existing_notice_data.emplace();
  }
  std::optional<base::Time> first_shown, last_shown;
  bool action_taken_found = false;
  for (const auto& event_ptr : existing_notice_data->notice_events) {
    if (event_ptr->event != kShown) {
      action_taken_found = true;
      continue;
    }
    if (!first_shown || *first_shown >= event_ptr->timestamp) {
      first_shown = event_ptr->timestamp;
    }
    if (!last_shown || *last_shown <= event_ptr->timestamp) {
      last_shown = event_ptr->timestamp;
    }
  }

  if (new_event == kShown) {
    RecordBool("NoticeShownForFirstTime", notice, !first_shown.has_value());
    return;
  }

  // At this point new_event is guaranteed to be an action.
  if (!first_shown) {
    RecordEnum("NoticeActionTakenBehavior", notice,
               NoticeActionBehavior::kActionBeforeShown);
  }
  if (action_taken_found) {
    RecordEnum("NoticeActionTakenBehavior", notice,
               NoticeActionBehavior::kDuplicateActionTaken);
  }
  if (!action_taken_found && first_shown) {
    RecordEnum("NoticeActionTakenBehavior", notice,
               NoticeActionBehavior::kSuccess);
  }
  base::Time event_time = base::Time::Now();
  if (first_shown) {
    RecordTimingWithAction("FirstShownToInteractedDuration", notice,
                           GetNoticeActionStringFromEvent(new_event),
                           event_time - *first_shown);
  }
  if (last_shown) {
    RecordTimingWithAction("LastShownToInteractedDuration", notice,
                           GetNoticeActionStringFromEvent(new_event),
                           event_time - *last_shown);
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

std::string GetNoticeActionStringFromEvent(Event event) {
  switch (event) {
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
    case kShown:
      NOTREACHED();
  }
}

std::optional<Event> NoticeActionToEvent(NoticeActionTaken action) {
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

  if (auto notice_event = NoticeActionToEvent(data_v1.notice_action_taken)) {
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
    PrefService* pref_service)
    : pref_service_(pref_service) {
  CHECK(pref_service_);
}

PrivacySandboxNoticeStorage::~PrivacySandboxNoticeStorage() = default;

void PrivacySandboxNoticeStorage::RecordStartupHistograms() const {
  for (const auto [notice, notice_value] :
       pref_service_->GetDict(kNoticeDataPath)) {
    auto notice_data = ConvertTo<NoticeStorageData>(&notice_value);
    if (!notice_data.has_value() || notice_data->notice_events.empty()) {
      continue;
    }
    RecordEnum("Startup.LastRecordedEvent", notice,
               notice_data->notice_events.back()->event);
  }
}

std::optional<NoticeStorageData> PrivacySandboxNoticeStorage::ReadNoticeData(
    std::string_view notice) const {
  const base::DictValue& pref_data = pref_service_->GetDict(kNoticeDataPath);
  return ConvertTo<NoticeStorageData>(pref_data.FindDict(notice));
}

void PrivacySandboxNoticeStorage::RecordEvent(const Notice& notice,
                                              Event event) {
  EmitNewEventHistograms(ReadNoticeData(notice.GetStorageName()),
                         notice.GetStorageName(), event);

  ScopedDictPrefUpdate update(pref_service_, kNoticeDataPath);
  base::DictValue* dict = update->EnsureDict(notice.GetStorageName());
  dict->Set(kSchemaVersionKey, kCurrentSchemaVersion);
  dict->Set(kChromeVersionKey, version_info::GetVersionNumber());
  dict->EnsureList(kEventsKey)->Append(BuildDictEntryEvent(event));
}

}  // namespace privacy_sandbox
