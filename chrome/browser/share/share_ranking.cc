// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/share/share_ranking.h"

#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/profiles/profile.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "content/public/browser/storage_partition.h"

namespace sharing {

namespace {

const char* const kShareRankingFolder = "share_ranking";

std::unique_ptr<ShareRanking::BackingDb> MakeDefaultDbForProfile(
    Profile* profile) {
  return profile->GetDefaultStoragePartition()
      ->GetProtoDatabaseProvider()
      ->GetDB<proto::ShareRanking>(
          leveldb_proto::ProtoDbType::SHARE_RANKING_DATABASE,
          profile->GetPath().Append(kShareRankingFolder),
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::TaskPriority::BEST_EFFORT}));
}

}  // namespace

ShareRanking::ShareRanking(Profile* profile,
                           std::unique_ptr<BackingDb> backing_db)
    : db_(backing_db ? std::move(backing_db)
                     : MakeDefaultDbForProfile(profile)) {
  Init();
}

ShareRanking::~ShareRanking() = default;

void ShareRanking::UpdateRanking(const std::string& type, Ranking ranking) {
  if (!init_finished_) {
    post_init_callbacks_.AddUnsafe(base::BindOnce(
        &ShareRanking::UpdateRanking, base::Unretained(this), type, ranking));
    return;
  }

  ranking_[type] = ranking;
  if (db_init_status_ == leveldb_proto::Enums::kOK)
    FlushToBackingDb(type);
}

void ShareRanking::GetRanking(const std::string& type,
                              GetRankingCallback callback) {
  if (!init_finished_) {
    post_init_callbacks_.AddUnsafe(base::BindOnce(&ShareRanking::GetRanking,
                                                  base::Unretained(this), type,
                                                  std::move(callback)));
    return;
  }

  if (db_init_status_ != leveldb_proto::Enums::kOK) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), absl::nullopt));
    return;
  }

  if (ranking_.contains(type)) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  absl::make_optional(ranking_[type])));
    return;
  }

  db_->GetEntry(type, base::BindOnce(&ShareRanking::OnBackingGetDone,
                                     weak_factory_.GetWeakPtr(), type,
                                     std::move(callback)));
}

void ShareRanking::Rank(ShareHistory* history,
                        const std::string& type,
                        const std::vector<std::string>& available_on_system,
                        int fold,
                        bool persist_update,
                        GetRankingCallback callback) {
  NOTIMPLEMENTED();
}

// static
void ShareRanking::ComputeRanking(
    const std::map<std::string, int>& all_share_history,
    const std::map<std::string, int>& recent_share_history,
    const Ranking& old_ranking,
    const std::vector<std::string>& available_on_system,
    int fold,
    Ranking* display_ranking,
    Ranking* persisted_ranking) {
  NOTIMPLEMENTED();
}

void ShareRanking::Init() {
  db_->Init(
      base::BindOnce(&ShareRanking::OnInitDone, weak_factory_.GetWeakPtr()));
}

void ShareRanking::OnInitDone(leveldb_proto::Enums::InitStatus status) {
  db_init_status_ = status;
  init_finished_ = true;
  post_init_callbacks_.Notify();
}

void ShareRanking::OnBackingGetDone(
    std::string key,
    GetRankingCallback callback,
    bool ok,
    std::unique_ptr<proto::ShareRanking> ranking) {
  if (!ok || db_init_status_ != leveldb_proto::Enums::kOK) {
    std::move(callback).Run(absl::nullopt);
    return;
  }

  ranking_[key].clear();
  for (const auto& it : ranking->targets()) {
    ranking_[key].push_back(it);
  }

  std::move(callback).Run(absl::make_optional(ranking_[key]));
}

void ShareRanking::FlushToBackingDb(const std::string& key) {
  proto::ShareRanking proto;
  for (const auto& element : ranking_[key]) {
    proto.mutable_targets()->Add(std::string(element));
  }

  auto keyvals = std::make_unique<BackingDb::KeyEntryVector>();
  keyvals->push_back({key, proto});
  db_->UpdateEntries(std::move(keyvals),
                     std::make_unique<std::vector<std::string>>(),
                     base::DoNothing());
}

}  // namespace sharing
