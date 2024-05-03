// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/chromebox_for_meetings/hotlog2/local_data_source.h"

#include "base/i18n/time_formatting.h"
#include "base/process/launch.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"

namespace ash::cfm {

namespace {

// Local convenience aliases
using mojom::DataFilter::FilterType::CHANGE;
using mojom::DataFilter::FilterType::REGEX;

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
  if (data_buffer_.empty() && pending_upload_buffer_.empty()) {
    // TODO(b/326441003): serialize output
    std::move(callback).Run({});
    return;
  }

  // Move the contents of the internal data buffer into the
  // pending upload buffer. If the pending upload buffer is not
  // empty (from a previously-failed upload attempt), do nothing
  // and attempt to consume it again.
  if (pending_upload_buffer_.empty()) {
    std::move(data_buffer_.begin(), data_buffer_.end(),
              std::back_inserter(pending_upload_buffer_));
    data_buffer_.clear();
  }

  std::move(callback).Run(pending_upload_buffer_);
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
  pending_upload_buffer_.clear();
}

void LocalDataSource::StartCollectingData() {
  poll_timer_.Start(FROM_HERE, poll_rate_,
                    base::BindRepeating(&LocalDataSource::FillDataBuffer,
                                        weak_ptr_factory_.GetWeakPtr()));
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

    // Update our last known unique data and add a timestamp. Note that
    // we're assuming non-incremental sources will not have their own
    // timestamps already prepended, which should hold true.
    last_unique_data_ = next_data;
    AddTimestamps(next_data);

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
  // TODO(b/326441003): add serialization logic
  (void)buffer;
}

void LocalDataSource::AddTimestamps(std::vector<std::string>& data) {
  auto formatted_time =
      base::TimeFormatAsIso8601(base::Time::NowFromSystemTime());
  for (size_t i = 0; i < data.size(); i++) {
    data[i] = formatted_time + " " + data[i];
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
