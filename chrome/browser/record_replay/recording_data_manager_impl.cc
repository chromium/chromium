// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/record_replay/recording_data_manager_impl.h"

#include "base/command_line.h"
#include "base/containers/map_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/types/optional_ref.h"
#include "chrome/browser/record_replay/recording.pb.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/leveldb_proto/public/proto_database_provider.h"

namespace record_replay {

namespace {
constexpr base::FilePath::StringViewType kRecordingsDatabaseFileName =
    FILE_PATH_LITERAL("Recordings");
}

RecordingDataManagerImpl::RecordingDataManagerImpl(
    leveldb_proto::ProtoDatabaseProvider* db_provider,
    const base::FilePath& profile_path) {
  scoped_refptr<base::SequencedTaskRunner> database_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});
  db_ = db_provider->GetDB<Recording>(
      leveldb_proto::ProtoDbType::RECORD_REPLAY_STORE,
      profile_path.Append(kRecordingsDatabaseFileName),
      std::move(database_task_runner));
  db_->Init(
      base::BindRepeating(&RecordingDataManagerImpl::OnDatabaseInitialized,
                          weak_ptr_factory_.GetWeakPtr()));
}

RecordingDataManagerImpl::~RecordingDataManagerImpl() = default;

void RecordingDataManagerImpl::OnDatabaseInitialized(
    leveldb_proto::Enums::InitStatus status) {
  if (status != leveldb_proto::Enums::InitStatus::kOK) {
    return;
  }
  db_is_initialized_ = true;
  db_->LoadKeysAndEntries(
      base::BindOnce(&RecordingDataManagerImpl::OnDatabaseLoadKeysAndEntries,
                     weak_ptr_factory_.GetWeakPtr()));
}

void RecordingDataManagerImpl::OnDatabaseLoadKeysAndEntries(
    bool success,
    std::unique_ptr<std::map<std::string, Recording>> entries) {
  if (!success) {
    return;
  }
  // TODO(b/476101114): Remove this hack once it's not needed anymore.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch("wipe-recordings")) {
    return;
  }
  url_to_record_ = std::move(*entries);
}

void RecordingDataManagerImpl::AddRecording(Recording recording) {
  if (!db_is_initialized_) {
    // This handles a race condition in ProtoDatabaseImpl where a sufficient
    // number of AddRecording() calls before OnDatabaseInitialized() hits a
    // DCHECK.
    // TODO(crbug.com/483687781): Remove this test once the the issue fixed.
    return;
  }
  auto entries_to_save = std::make_unique<
      leveldb_proto::ProtoDatabase<Recording>::KeyEntryVector>();
  auto keys_to_remove = std::make_unique<std::vector<std::string>>();
  entries_to_save->emplace_back(recording.url(), recording);
  db_->UpdateEntries(std::move(entries_to_save), std::move(keys_to_remove),
                     /*callback=*/base::DoNothing());
  url_to_record_[recording.url()] = std::move(recording);
}

base::optional_ref<const Recording> RecordingDataManagerImpl::GetRecording(
    const std::string& url) const LIFETIME_BOUND {
  return base::FindOrNull(url_to_record_, url);
}

}  // namespace record_replay
