// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/chromebox_for_meetings/hotlog2/command_source.h"
#include "base/strings/string_split.h"

namespace ash::cfm {

CommandSource::CommandSource(std::string command) : command_(command) {
  command_split_ = base::SplitString(command, " ", base::KEEP_WHITESPACE,
                                     base::SPLIT_WANT_NONEMPTY);
}

inline CommandSource::~CommandSource() = default;

void CommandSource::Fetch(FetchCallback callback) {
  // TODO: (b/326440931)
  (void)callback;
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
