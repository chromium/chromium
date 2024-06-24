// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/chromebox_for_meetings/artemis/local_data_source.h"

#include "base/hash/hash.h"
#include "base/i18n/time_formatting.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"

namespace ash::cfm {

namespace {

// Local convenience aliases
using mojom::DataFilter::FilterType::CHANGE;
using mojom::DataFilter::FilterType::REGEX;

// Regex used to separate timestamps and severity from rest of data.
// Note that the timestamp and severity fields are both optional fields.
// In the event that a data source produces data that doesn't have one
// or the other, defaults will be provided.
constexpr LazyRE2 kFullLogLineRegex = {
    "^(?:([0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9:\\.]+Z) )?"
    "(?:(EMERG|ALERT|CRIT|SEVERE|ERR|ERROR|WARNING|INFO|NOTICE"
    "|DEBUG|VERBOSE[1-4]) )?((?s).*)"};

// Number of characters to ingest per log line to create unique hash.
constexpr size_t kLogMsgHashSize = 50;

}  // namespace

LocalDataSource::LocalDataSource(base::TimeDelta poll_rate,
                                 bool data_needs_redacting,
                                 bool is_incremental)
    : poll_rate_(poll_rate),
      data_needs_redacting_(data_needs_redacting),
      is_incremental_(is_incremental),
      redactor_(nullptr) {}

inline LocalDataSource::~LocalDataSource() = default;

void LocalDataSource::Fetch(FetchCallback callback) {
  if (data_buffer_.empty()) {
    std::move(callback).Run({});
    return;
  }

  // data_buffer_ is a deque, so move data to a proper vector first.
  std::vector<std::string> return_data;
  std::move(data_buffer_.begin(), data_buffer_.end(),
            std::back_inserter(return_data));
  data_buffer_.clear();

  std::move(callback).Run(std::move(return_data));
}

void LocalDataSource::AddWatchDog(
    mojom::DataFilterPtr filter,
    mojo::PendingRemote<mojom::DataWatchDog> pending_watch_dog,
    AddWatchDogCallback callback) {
  if (!IsWatchDogFilterValid(filter)) {
    std::move(callback).Run(false /* success */);
    return;
  }

  mojo::Remote<mojom::DataWatchDog> remote(std::move(pending_watch_dog));
  std::string watchdog_name;

  if (filter->filter_type == CHANGE) {
    // Trigger CHANGE watchdogs immediately with the last known data.
    const std::string data_joined = base::JoinString(last_unique_data_, "\n");
    remote->OnNotify(data_joined);

    change_based_watchdogs_.Add(std::move(remote));
    watchdog_name = "CHANGE";
  } else {
    // Pattern is guaranteed to be populated based on the
    // results of IsWatchDogFilterValid().
    const std::string& pattern = filter->pattern.value();
    if (regex_cache_.count(pattern) == 0) {
      regex_cache_[pattern] = std::make_unique<RE2>(pattern);
    }

    regex_based_watchdogs_[pattern].Add(std::move(remote));
    watchdog_name = pattern;
  }

  VLOG(4) << "Watchdog added to '" << GetDisplayName() << "'; will match on "
          << watchdog_name;
  std::move(callback).Run(true /* success */);
}

void LocalDataSource::Flush() {
  // No-op by default. Use this function to perform any cleanup
  // task needed after a successful processing step.
  return;
}

void LocalDataSource::StartCollectingData() {
  poll_timer_.Start(FROM_HERE, poll_rate_,
                    base::BindRepeating(&LocalDataSource::FillDataBuffer,
                                        weak_ptr_factory_.GetWeakPtr()));
}

void LocalDataSource::AssignDeviceID(const std::string& id) {
  device_id_ = id;
}

void LocalDataSource::FillDataBuffer() {
  std::vector<std::string> next_data = GetNextData();
  if (next_data.empty()) {
    return;
  }

  if (!is_incremental_) {
    // If non-incremental sources (e.g. commands) return the same
    // data as the previous invocation, return early. This will
    // skip any watchdog checks and will prevent the duplicate
    // data from eventually being enqueued to the cloud logger.
    // Note that incremental sources (e.g. logs) will always have
    // new data, so we don't need to check for duplicates.
    if (next_data == last_unique_data_) {
      return;
    }

    // Update our last known unique data.
    last_unique_data_ = next_data;

    // Fire any CHANGE watchdogs. Note that these are not supported
    // for incremental sources as they will always be changing.
    if (!change_based_watchdogs_.empty()) {
      // The OnNotify() callbacks expect a single string of data, so join
      // the vector first.
      const std::string data_joined = base::JoinString(next_data, "\n");
      FireChangeWatchdogCallbacks(data_joined);
    }
  }

  for (const auto& line : next_data) {
    CheckRegexWatchdogsAndFireCallbacks(line);
  }

  if (data_needs_redacting_) {
    RedactDataBuffer(next_data);
  }

  SerializeDataBuffer(next_data);

  std::move(next_data.begin(), next_data.end(),
            std::back_inserter(data_buffer_));

  // We're over our limit, so purge old logs until we're not.
  if (IsDataBufferOverMaxLimit()) {
    LOG(WARNING) << "Data buffer full for '" << GetDisplayName()
                 << "'. Purging older records.";
    int dropped_records = 0;

    while (IsDataBufferOverMaxLimit()) {
      data_buffer_.pop_front();
      dropped_records++;
    }

    LOG(WARNING) << "Dropped " << dropped_records << " records.";
  }
}

bool LocalDataSource::IsDataBufferOverMaxLimit() {
  return data_buffer_.size() > kMaxInternalBufferSize;
}

void LocalDataSource::RedactDataBuffer(std::vector<std::string>& buffer) {
  for (size_t i = 0; i < buffer.size(); i++) {
    buffer[i] = redactor_.Redact(buffer[i]);
  }
}

void LocalDataSource::SerializeDataBuffer(std::vector<std::string>& buffer) {
  if (buffer.empty()) {
    return;
  }

  // Set defaults for data that doesn't have timestamps or severity
  // levels. Use the current time for the timestamp.
  auto default_timestamp =
      (base::Time::Now() - base::Time::UnixEpoch()).InMicroseconds();
  const proto::LogSeverity default_severity = proto::LOG_SEVERITY_DEFAULT;

  // Build each LogEntry object, then replace the buffer data with
  // the serialized entry.
  for (size_t i = 0; i < buffer.size(); i++) {
    proto::LogEntry entry;
    BuildLogEntryFromLogLine(entry, buffer[i], default_timestamp,
                             default_severity);

    std::string serialized_data;
    entry.SerializeToString(&serialized_data);
    buffer[i] = std::move(serialized_data);
  }
}

void LocalDataSource::BuildLogEntryFromLogLine(
    proto::LogEntry& entry,
    const std::string& line,
    const uint64_t default_timestamp,
    const proto::LogSeverity& default_severity) {
  std::string timestamp;
  std::string severity;
  std::string log_msg;

  RE2& regex = GetLogLineRegex();

  if (!RE2::PartialMatch(line, regex, &timestamp, &severity, &log_msg)) {
    LOG(ERROR) << "Unable to parse log line properly: " << line;
    entry.set_timestamp_micros(default_timestamp);
    entry.set_severity(default_severity);
    entry.set_text_payload(line);
  } else {
    uint64_t time_since_epoch;

    // There are three cases to consider here:
    // 1. The timestamp is populated and was parsed properly. Pass
    //    to TimestampStringToUnixTime() to convert to Unix epoch.
    // 2. The source explicitly does not expect timestamps to be
    //    present. Apply the passed-in default. Note that most (if
    //    not all) non-incremental sources fall under this category.
    // 3. The timestamp is not populated, and this is a data source
    //    that expects timestamps to be present, so this line is
    //    likely a new line from a previous log. Carry forward the
    //    last timestamp that was recorded, plus 1 microsecond.
    if (!timestamp.empty()) {
      time_since_epoch = TimestampStringToUnixTime(timestamp);
      if (time_since_epoch != 0) {
        last_recorded_timestamp_ = time_since_epoch;
      }
    } else if (!AreTimestampsExpected()) {
      time_since_epoch = default_timestamp;
    } else {
      time_since_epoch = ++last_recorded_timestamp_;
    }

    // Use the log source and timestamp to create a unique ID that can be
    // used to identify this entry. This will aid in de-duplication on the
    // server side.
    if (is_incremental_ && time_since_epoch != 0) {
      entry.set_insert_id(GetUniqueInsertId(log_msg));
    }

    // Even if the match succeeded, the timestamps and severity are optional
    // matches, so supply a default if they aren't populated.
    entry.set_timestamp_micros(time_since_epoch);
    entry.set_severity(severity.empty() ? default_severity
                                        : SeverityStringToEnum(severity));
    entry.set_text_payload(log_msg);
  }
}

const std::string LocalDataSource::GetUniqueInsertId(
    const std::string& log_msg) {
  std::string to_be_hashed = device_id_ + ":" + GetDisplayName() + ":" +
                             log_msg.substr(0, kLogMsgHashSize);
  size_t hash = base::FastHash(to_be_hashed);
  return base::NumberToString(hash);
}

RE2& LocalDataSource::GetLogLineRegex() {
  // Default regex. Data sources can override this if necessary.
  // See notes in local_data_source.h for restrictions.
  return *kFullLogLineRegex;
}

uint64_t LocalDataSource::TimestampStringToUnixTime(
    const std::string& timestamp) {
  base::Time time;
  if (!base::Time::FromString(timestamp.c_str(), &time)) {
    LOG(ERROR) << "Unable to parse timestamp: " << timestamp;
    return 0;
  }
  return (time - base::Time::UnixEpoch()).InMicroseconds();
}

bool LocalDataSource::AreTimestampsExpected() const {
  // By default, assume that non-incremental sources will not supply
  // timestamps, and assume the opposite for incremental sources. Data
  // sources that do not follow this rule should override this function.
  return is_incremental_;
}

proto::LogSeverity LocalDataSource::SeverityStringToEnum(
    const std::string& severity) {
  if (severity == "EMERGENCY" || severity == "EMERG") {
    return proto::LOG_SEVERITY_EMERGENCY;
  } else if (severity == "ALERT") {
    return proto::LOG_SEVERITY_ALERT;
  } else if (severity == "CRITICAL" || severity == "CRIT") {
    return proto::LOG_SEVERITY_CRITICAL;
  } else if (severity == "ERROR" || severity == "ERR") {
    return proto::LOG_SEVERITY_ERROR;
  } else if (severity == "WARNING") {
    return proto::LOG_SEVERITY_WARNING;
  } else if (severity == "NOTICE") {
    return proto::LOG_SEVERITY_NOTICE;
  } else if (severity == "INFO") {
    return proto::LOG_SEVERITY_INFO;
  } else if (severity == "DEBUG" || severity == "VERBOSE1" ||
             severity == "VERBOSE2" || severity == "VERBOSE3" ||
             severity == "VERBOSE4") {
    return proto::LOG_SEVERITY_DEBUG;
  } else {
    LOG(ERROR) << "Unable to parse severity: " << severity;
    return proto::LOG_SEVERITY_DEFAULT;
  }
}

bool LocalDataSource::IsWatchDogFilterValid(mojom::DataFilterPtr& filter) {
  if (filter->filter_type != CHANGE && filter->filter_type != REGEX) {
    LOG(ERROR) << "Somehow received a DataFilter of unknown type "
               << filter->filter_type;
    return false;
  }

  if (filter->filter_type == CHANGE && is_incremental_) {
    LOG(ERROR) << "Incremental sources do not support change-based watchdogs";
    return false;
  }

  if (filter->filter_type == REGEX) {
    if (filter->pattern.value_or("").empty()) {
      LOG(ERROR) << "Regex watchdog was requested, but the pattern is empty";
      return false;
    }

    const std::string& pattern = filter->pattern.value();

    if (pattern == "*") {
      LOG(ERROR) << "Pattern '*' is too loose. Use CHANGE watchdog instead.";
      return false;
    }

    RE2 test_regex(pattern);

    if (!test_regex.ok()) {
      LOG(ERROR) << "Regex '" << pattern
                 << "' is invalid (err: " << test_regex.error() << ")";
      return false;
    }

  } else if (filter->filter_type == CHANGE) {
    if (filter->pattern.has_value()) {
      LOG(ERROR) << "CHANGE filter requested with pattern";
      return false;
    }
  }

  return true;
}

void LocalDataSource::FireChangeWatchdogCallbacks(const std::string& data) {
  VLOG(4) << "'" << GetDisplayName()
          << "' matched on 'CHANGE' watchdog. Notifying observers.";
  for (const auto& remote : change_based_watchdogs_) {
    remote->OnNotify(data);
  }
}

void LocalDataSource::CheckRegexWatchdogsAndFireCallbacks(
    const std::string& data) {
  for (const auto& it : regex_based_watchdogs_) {
    const auto& pattern = it.first;
    const auto& remotes = it.second;

    if (!RE2::PartialMatch(data, *regex_cache_[pattern])) {
      continue;
    }

    VLOG(4) << "'" << GetDisplayName() << "' matched on '" << pattern
            << "' watchdog. Notifying observers.";

    for (auto& remote : remotes) {
      remote->OnNotify(data);
    }
  }
}

}  // namespace ash::cfm
