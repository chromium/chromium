// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/chromebox_for_meetings/hotlog2/local_data_source.h"

#include "base/i18n/time_formatting.h"
#include "base/process/launch.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"

namespace ash::cfm {

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

    if (data_needs_redacting_) {
      RedactUploadBuffer();
    }
    // TODO(b/326441003): serialize upload buffer
  }

  std::move(callback).Run(pending_upload_buffer_);
}

void LocalDataSource::AddWatchDog(
    mojom::DataFilterPtr filter,
    mojo::PendingRemote<mojom::DataWatchDog> pending_watch_dog,
    AddWatchDogCallback callback) {
  // TODO: (b/326440932)
  (void)pending_watch_dog;
  std::move(callback).Run(false /* success */);
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
    if (next_data == last_unique_data_) {
      return;
    }

    // Update our last known unique data and add a timestamp. Note that
    // we're assuming non-incremental sources will not have their own
    // timestamps already prepended, which should hold true.
    last_unique_data_ = next_data;
    AddTimestamps(next_data);
  }

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

void LocalDataSource::RedactUploadBuffer() {
  for (size_t i = 0; i < pending_upload_buffer_.size(); i++) {
    pending_upload_buffer_[i] = redactor_.Redact(pending_upload_buffer_[i]);
  }
}

void LocalDataSource::AddTimestamps(std::vector<std::string>& data) {
  auto formatted_time =
      base::TimeFormatAsIso8601(base::Time::NowFromSystemTime());
  for (size_t i = 0; i < data.size(); i++) {
    data[i] = formatted_time + " " + data[i];
  }
}

}  // namespace ash::cfm
