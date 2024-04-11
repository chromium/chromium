// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/chromebox_for_meetings/hotlog2/local_data_source.h"

#include "base/process/launch.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"

namespace ash::cfm {

// Maximum lines that can be in the internal buffer before we start
// purging older records. In the working case, we should never hit
// this limit, but we may reach it if we're unable to enqueue logs
// via Fetch() for whatever reason (eg a network outage).
constexpr int kMaxInternalBufferSize = 50000;  // ~7Mb

LocalDataSource::LocalDataSource(base::TimeDelta poll_rate)
    : poll_rate_(poll_rate) {}

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
    // TODO(b/327020292): redact data
    // TODO(b/326441003): serialize data
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
  // TODO: (b/326440932)
  (void)pending_watch_dog;
  std::move(callback).Run(false /* success */);
}

void LocalDataSource::Flush() {
  pending_upload_buffer_.clear();
}

void LocalDataSource::StartPollTimer() {
  poll_timer_.Start(FROM_HERE, poll_rate_,
                    base::BindRepeating(&LocalDataSource::FillDataBuffer,
                                        weak_ptr_factory_.GetWeakPtr()));
}

void LocalDataSource::FillDataBuffer() {
  std::vector<std::string> next_data = GetNextData();
  if (next_data.empty()) {
    return;
  }

  std::move(next_data.begin(), next_data.end(),
            std::back_inserter(data_buffer_));

  // We're over our limit, so purge old logs until we're not.
  if (IsDataBufferAtMaxLimit()) {
    LOG(WARNING) << "Data buffer full for '" << GetDisplayName()
                 << "'. Purging older records.";
    int dropped_records = 0;

    while (IsDataBufferAtMaxLimit()) {
      data_buffer_.pop_front();
      dropped_records++;
    }

    LOG(WARNING) << "Dropped " << dropped_records << " records.";
  }
}

bool LocalDataSource::IsDataBufferAtMaxLimit() {
  return data_buffer_.size() >= kMaxInternalBufferSize;
}

}  // namespace ash::cfm
