// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RECORD_REPLAY_RECORDING_DATA_MANAGER_IMPL_H_
#define CHROME_BROWSER_RECORD_REPLAY_RECORDING_DATA_MANAGER_IMPL_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/types/optional_ref.h"
#include "chrome/browser/record_replay/recording.pb.h"
#include "chrome/browser/record_replay/recording_data_manager.h"
#include "components/leveldb_proto/public/proto_database.h"

namespace leveldb_proto {
class ProtoDatabaseProvider;
}  // namespace leveldb_proto

namespace record_replay {

// One instance per BrowserContext.
class RecordingDataManagerImpl : public RecordingDataManager {
 public:
  explicit RecordingDataManagerImpl(
      leveldb_proto::ProtoDatabaseProvider* db_provider,
      const base::FilePath& profile_path);
  RecordingDataManagerImpl(const RecordingDataManagerImpl&) = delete;
  RecordingDataManagerImpl& operator=(const RecordingDataManagerImpl&) = delete;
  ~RecordingDataManagerImpl() override;

  void AddRecording(Recording recording) override;
  base::optional_ref<const Recording> GetRecording(const std::string& url) const
      LIFETIME_BOUND override;

 private:
  void OnDatabaseInitialized(leveldb_proto::Enums::InitStatus status);
  void OnDatabaseLoadKeysAndEntries(
      bool success,
      std::unique_ptr<std::map<std::string, Recording>> entries);

  std::unique_ptr<leveldb_proto::ProtoDatabase<Recording>> db_;
  bool db_is_initialized_ = false;
  std::map<std::string, Recording> url_to_record_;
  base::WeakPtrFactory<RecordingDataManagerImpl> weak_ptr_factory_{this};
};

}  // namespace record_replay

#endif  // CHROME_BROWSER_RECORD_REPLAY_RECORDING_DATA_MANAGER_IMPL_H_
