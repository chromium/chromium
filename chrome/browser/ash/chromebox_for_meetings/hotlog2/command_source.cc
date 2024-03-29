// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/chromebox_for_meetings/hotlog2/command_source.h"

#include "base/process/launch.h"
#include "base/strings/string_split.h"

namespace ash::cfm {

CommandSource::CommandSource(const std::string& command) : command_(command) {
  command_split_ = base::SplitString(command, " ", base::KEEP_WHITESPACE,
                                     base::SPLIT_WANT_NONEMPTY);
}

inline CommandSource::~CommandSource() = default;

void CommandSource::Fetch(FetchCallback callback) {
  std::string output;
  base::GetAppOutputAndError(command_split_, &output);
  std::vector<std::string> output_split = base::SplitString(
      output, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  // TODO(b/327020292): redact output
  // TODO(b/326441003): serialize output
  std::move(callback).Run(output_split);
}

void CommandSource::AddWatchDog(
    mojo::PendingRemote<mojom::DataWatchDog> watch_dog) {
  // TODO: (b/326440932)
  (void)watch_dog;
}

void CommandSource::Flush() {
  // TODO: (b/326440931)
  return;
}

}  // namespace ash::cfm
