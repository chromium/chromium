// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/1226246): Implement this for Fuchsia.

#include "chrome/browser/memory_details.h"

#include "base/notreached.h"

MemoryDetails::MemoryDetails() {
  NOTIMPLEMENTED_LOG_ONCE();

  // Populate |process_data_| with a single empty entry, so that the
  // ChromeBrowser() accessor, below, can return something.
  process_data_.resize(1);
}

ProcessData* MemoryDetails::ChromeBrowser() {
  NOTIMPLEMENTED_LOG_ONCE();
  return &process_data_[0];
}

void MemoryDetails::CollectProcessData(
    const std::vector<ProcessMemoryInformation>& child_info) {
  NOTIMPLEMENTED_LOG_ONCE();
}
