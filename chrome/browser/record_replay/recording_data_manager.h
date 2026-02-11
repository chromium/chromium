// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RECORD_REPLAY_RECORDING_DATA_MANAGER_H_
#define CHROME_BROWSER_RECORD_REPLAY_RECORDING_DATA_MANAGER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/types/optional_ref.h"
#include "chrome/browser/record_replay/recording.pb.h"
#include "components/keyed_service/core/keyed_service.h"

namespace record_replay {

// Stores the recording protos.
class RecordingDataManager : public KeyedService {
 public:
  RecordingDataManager() = default;
  RecordingDataManager(const RecordingDataManager&) = delete;
  RecordingDataManager& operator=(const RecordingDataManager&) = delete;
  RecordingDataManager(RecordingDataManager&&) = delete;
  RecordingDataManager& operator=(RecordingDataManager&&) = delete;
  ~RecordingDataManager() override = default;

  virtual void AddRecording(Recording recording) = 0;
  virtual base::optional_ref<const Recording> GetRecording(
      const std::string& url) const LIFETIME_BOUND = 0;
};

}  // namespace record_replay

#endif  // CHROME_BROWSER_RECORD_REPLAY_RECORDING_DATA_MANAGER_H_
