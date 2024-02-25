// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/test/test_utils.h"

#include <sstream>
#include <utility>

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/notifications/scheduler/internal/notification_entry.h"
#include "chrome/browser/notifications/scheduler/public/notification_data.h"

namespace notifications {
namespace test {

ImpressionTestData::ImpressionTestData(
    SchedulerClientType type,
    size_t current_max_daily_show,
    std::vector<Impression> impressions,
    std::optional<SuppressionInfo> suppression_info,
    size_t negative_events_count,
    std::optional<base::Time> last_negative_event_ts,
    std::optional<base::Time> last_shown_ts)
    : type(type),
      current_max_daily_show(current_max_daily_show),
      impressions(std::move(impressions)),
      suppression_info(std::move(suppression_info)),
      negative_events_count(negative_events_count),
      last_negative_event_ts(last_negative_event_ts),
      last_shown_ts(last_shown_ts) {}

ImpressionTestData::ImpressionTestData(const ImpressionTestData& other) =
    default;

ImpressionTestData::~ImpressionTestData() = default;

void AddImpressionTestData(const ImpressionTestData& data,
                           ClientState* client_state) {
  DCHECK(client_state);
  client_state->type = data.type;
  client_state->current_max_daily_show = data.current_max_daily_show;
  for (const auto& impression : data.impressions) {
    client_state->impressions.emplace_back(impression);
  }
  client_state->suppression_info = data.suppression_info;
  client_state->negative_events_count = data.negative_events_count;
  client_state->last_shown_ts = data.last_shown_ts;
  client_state->last_negative_event_ts = data.last_negative_event_ts;
}

void AddImpressionTestData(
    const std::vector<ImpressionTestData>& test_data_vec,
    ImpressionHistoryTracker::ClientStates* client_states) {
  DCHECK(client_states);
  for (const auto& test_data : test_data_vec) {
    auto client_state = std::make_unique<ClientState>();
    AddImpressionTestData(test_data, client_state.get());
    client_states->emplace(test_data.type, std::move(client_state));
  }
}

void AddImpressionTestData(
    const std::vector<ImpressionTestData>& test_data_vec,
    std::vector<std::unique_ptr<ClientState>>* client_states) {
  DCHECK(client_states);
  for (const auto& test_data : test_data_vec) {
    auto client_state = std::make_unique<ClientState>();
    AddImpressionTestData(test_data, client_state.get());
    client_states->emplace_back(std::move(client_state));
  }
}

Impression CreateImpression(const base::Time& create_time,
                            UserFeedback feedback,
                            ImpressionResult impression_result,
                            bool integrated,
                            const std::string& guid,
                            SchedulerClientType type) {
  Impression impression(type, guid, create_time);
  impression.feedback = feedback;
  impression.impression = impression_result;
  impression.integrated = integrated;
  return impression;
}

std::string DebugString(const NotificationData* data) {
  DCHECK(data);
  std::ostringstream stream;
  stream << " Notification Data:"
         << " \n title:" << data->title << "\n message:" << data->message
         << " \n custom data: ";
  for (const auto& pair : data->custom_data)
    stream << " key:" << pair.first << " , value:" << pair.second;

  return stream.str();
}

std::string DebugString(const NotificationEntry* entry) {
  DCHECK(entry);
  std::ostringstream stream;
  stream << "NotificationEntry: \n  type: " << static_cast<int>(entry->type)
         << " \n guid: " << entry->guid << "\n create_time: "
         << entry->create_time.ToDeltaSinceWindowsEpoch().InMicroseconds()
         << " \n notification_data:" << DebugString(&entry->notification_data)
         << " \n schedule params: priority:"
         << static_cast<int>(entry->schedule_params.priority);

  for (const auto& mapping : entry->schedule_params.impression_mapping) {
    stream << " \n impression mapping: " << static_cast<int>(mapping.first)
           << " : " << static_cast<int>(mapping.second);
  }

  if (base::Contains(entry->icons_uuid, IconType::kSmallIcon))
    stream << " \n small_icons_id:"
           << entry->icons_uuid.at(IconType::kSmallIcon);
  if (base::Contains(entry->icons_uuid, IconType::kLargeIcon))
    stream << " \n large_icons_id:"
           << entry->icons_uuid.at(IconType::kLargeIcon);

  return stream.str();
}

std::string DebugString(const ClientState* client_state) {
  DCHECK(client_state);
  std::string log = base::StringPrintf(
      "Client state: type: %d \n"
      "current_max_daily_show: %d \n"
      "impressions.size(): %zu \n"
      "negative_events_count: %zu \n",
      static_cast<int>(client_state->type),
      client_state->current_max_daily_show, client_state->impressions.size(),
      client_state->negative_events_count);

  if (client_state->last_negative_event_ts.has_value()) {
    std::ostringstream stream;
    stream << "last negative event timestamp: ",
        client_state->last_negative_event_ts.value();
    log += stream.str();
  }

  if (client_state->last_shown_ts.has_value()) {
    std::ostringstream stream;
    stream << "last shown notification timestamp: ",
        client_state->last_shown_ts.value();
    log += stream.str();
  }

  for (const auto& impression : client_state->impressions) {
    std::ostringstream stream;
    stream << "\n"
           << "Impression, create_time:" << impression.create_time << "\n"
           << " create_time in microseconds:"
           << impression.create_time.ToDeltaSinceWindowsEpoch().InMicroseconds()
           << "\n"
           << "feedback: " << static_cast<int>(impression.feedback) << "\n"
           << "impression result: " << static_cast<int>(impression.impression)
           << " \n"
           << "integrated: " << impression.integrated << "\n"
           << "guid: " << impression.guid << "\n"
           << "type: " << static_cast<int>(impression.type);

    for (const auto& mapping : impression.impression_mapping) {
      stream << " \n impression mapping: " << static_cast<int>(mapping.first)
             << " : " << static_cast<int>(mapping.second);
    }

    for (const auto& pair : impression.custom_data) {
      stream << " \n custom data, key: " << pair.first
             << " value: " << pair.second;
    }

    log += stream.str();
  }

  if (client_state->suppression_info.has_value()) {
    std::ostringstream stream;
    stream << "\n Suppression info, last_trigger_time: "
           << client_state->suppression_info->last_trigger_time << "\n"
           << "duration:" << client_state->suppression_info->duration << "\n"
           << "recover_goal:" << client_state->suppression_info->recover_goal;
    log += stream.str();
  }
  return log;
}

}  // namespace test
}  // namespace notifications
