// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RECORD_REPLAY_RECORDING_DATA_MANAGER_IMPL_H_
#define CHROME_BROWSER_RECORD_REPLAY_RECORDING_DATA_MANAGER_IMPL_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/threading/sequence_bound.h"
#include "chrome/browser/record_replay/capabilities_database.h"
#include "chrome/browser/record_replay/recording.pb.h"
#include "chrome/browser/record_replay/recording_data_manager.h"

namespace record_replay {

// Concrete implementation for `RecordingDataManager` using a LevelDB-backed
// database to save and load `Recording` protos by URL.
//
// Owned by `RecordingDataManagerFactory` as a `KeyedService`, and thus tied to
// the lifecycle of a `Profile`.
// It runs on the UI thread but uses `leveldb_proto` for background database
// I/O.
class RecordingDataManagerImpl : public RecordingDataManager {
 public:
  explicit RecordingDataManagerImpl(base::FilePath profile_path);
  RecordingDataManagerImpl(const RecordingDataManagerImpl&) = delete;
  RecordingDataManagerImpl& operator=(const RecordingDataManagerImpl&) = delete;
  ~RecordingDataManagerImpl() override;

  // RecordingDataManager:
  void AddRecording(Recording recording) override;
  void GetRecordingsByUrl(
      std::string url,
      base::OnceCallback<void(std::vector<Recording>)> callback) override;

 private:
  base::SequenceBound<CapabilitiesDatabase> db_;
};

}  // namespace record_replay

#endif  // CHROME_BROWSER_RECORD_REPLAY_RECORDING_DATA_MANAGER_IMPL_H_
