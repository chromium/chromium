// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RECORD_REPLAY_RECORDING_DATA_MANAGER_H_
#define CHROME_BROWSER_RECORD_REPLAY_RECORDING_DATA_MANAGER_H_

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "chrome/browser/record_replay/recording.pb.h"
#include "components/keyed_service/core/keyed_service.h"

namespace record_replay {

// Manages persistent storage for recording protos.
//
// Tied to the lifecycle of a `Profile`.
class RecordingDataManager : public KeyedService {
 public:
  RecordingDataManager() = default;
  RecordingDataManager(const RecordingDataManager&) = delete;
  RecordingDataManager& operator=(const RecordingDataManager&) = delete;
  RecordingDataManager(RecordingDataManager&&) = delete;
  RecordingDataManager& operator=(RecordingDataManager&&) = delete;
  ~RecordingDataManager() override = default;

  // Adds a recording to the database.
  virtual void AddRecording(Recording recording) = 0;

  // Retrieves every Recording that matches the given `url`.
  virtual void GetRecordingsByUrl(
      std::string url,
      base::OnceCallback<void(std::vector<Recording>)> callback) = 0;
};

}  // namespace record_replay

#endif  // CHROME_BROWSER_RECORD_REPLAY_RECORDING_DATA_MANAGER_H_
