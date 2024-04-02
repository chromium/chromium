// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/chromebox_for_meetings/hotlog2/command_source.h"

#include "base/process/launch.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"

namespace ash::cfm {

CommandSource::CommandSource(const std::string& command,
                             base::TimeDelta poll_rate)
    : command_(command), poll_rate_(poll_rate) {
  command_split_ = base::SplitString(command, " ", base::KEEP_WHITESPACE,
                                     base::SPLIT_WANT_NONEMPTY);

  StartPollTimer();
}

inline CommandSource::~CommandSource() = default;

void CommandSource::Fetch(FetchCallback callback) {
  if (command_buffer_.empty() && pending_upload_buffer_.empty()) {
    // TODO(b/326441003): serialize output
    std::move(callback).Run({});
    return;
  }

  std::vector<std::string> return_buffer;

  // Move the contents of the internal command buffer into the
  // pending upload buffer. If the pending upload buffer is not
  // empty (from a previously-failed upload attempt), do nothing
  // and attempt to consume it again.
  if (pending_upload_buffer_.empty()) {
    pending_upload_buffer_.swap(command_buffer_);
  }

  for (const auto& output : pending_upload_buffer_) {
    std::vector<std::string> output_split = base::SplitString(
        output, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

    // TODO(b/327020292): redact output
    // TODO(b/326441003): serialize output
    std::move(output_split.begin(), output_split.end(),
              std::back_inserter(return_buffer));
  }

  std::move(callback).Run(return_buffer);
}

void CommandSource::AddWatchDog(
    mojo::PendingRemote<mojom::DataWatchDog> watch_dog,
    AddWatchDogCallback callback) {
  // TODO: (b/326440932)
  (void)watch_dog;
  (void)callback;
}

void CommandSource::Flush() {
  pending_upload_buffer_.clear();
}

void CommandSource::StartPollTimer() {
  poll_timer_.Start(
      FROM_HERE, poll_rate_,
      base::BindRepeating(&CommandSource::StoreCommandOutputIfDifferent,
                          weak_ptr_factory_.GetWeakPtr()));
}

void CommandSource::StoreCommandOutputIfDifferent() {
  std::string output;
  base::GetAppOutputAndError(command_split_, &output);

  if (output == last_output_) {
    return;
  }

  // TODO(b/326440932): if there are CHANGE watchdogs, trigger them here.

  last_output_ = output;
  command_buffer_.push_back(output);
}

}  // namespace ash::cfm
