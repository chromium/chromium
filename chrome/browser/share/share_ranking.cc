// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/share/share_ranking.h"

#include "base/strings/string_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/profiles/profile.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "content/public/browser/storage_partition.h"

namespace sharing {

const char* const ShareRanking::kMoreTarget = "$more";

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

bool RankingContains(const std::vector<std::string>& ranking,
                     const std::string& element,
                     unsigned int upto_index = UINT_MAX) {
  for (unsigned int i = 0; i < ranking.size() && i < upto_index; ++i) {
    if (ranking[i] == element)
      return true;
  }
  return false;
}

std::vector<std::string> RankByUses(const std::map<std::string, int>& uses) {
  std::vector<std::string> ranked_uses;
  std::transform(uses.begin(), uses.end(), std::back_inserter(ranked_uses),
                 [](const auto& entry) { return entry.first; });
  std::sort(ranked_uses.begin(), ranked_uses.end(),
            [&](const std::string& a, const std::string& b) {
              return uses.at(a) < uses.at(b);
            });
  return ranked_uses;
}

std::string HighestUnshown(const std::vector<std::string>& shown,
                           const std::map<std::string, int>& uses,
                           unsigned int fold) {
  std::vector<std::string> ranked = RankByUses(uses);
  auto in_shown_above_fold = [&](const std::string& e) {
    return RankingContains(shown, e, fold);
  };

  ranked.erase(
      std::remove_if(ranked.begin(), ranked.end(), in_shown_above_fold),
      ranked.end());
  return ranked.empty() ? "" : ranked.front();
}

void SwapRankingElement(std::vector<std::string>& ranking,
                        const std::string& from,
                        const std::string& to) {
  DCHECK(RankingContains(ranking, from));
  DCHECK(RankingContains(ranking, to));

  auto from_loc = std::find(ranking.begin(), ranking.end(), from);
  auto to_loc = std::find(ranking.begin(), ranking.end(), to);
  *from_loc = to;
  *to_loc = from;
}

std::vector<std::string> ReplaceUnavailableEntries(
    const std::vector<std::string>& ranking,
    const std::vector<std::string>& available) {
  std::vector<std::string> result;
  std::transform(ranking.begin(), ranking.end(), std::back_inserter(result),
                 [&](const std::string& e) {
                   return RankingContains(available, e) ? e : "";
                 });
  return result;
}

void FillGaps(std::vector<std::string>& ranking,
              const std::vector<std::string>& available,
              unsigned int fold) {
  std::vector<std::string> unused_available = available;

  unused_available.erase(
      std::remove_if(unused_available.begin(), unused_available.end(),
                     [&](const std::string& e) {
                       return RankingContains(ranking, e, fold);
                     }),
      unused_available.end());
  auto next_unused = unused_available.begin();

  DCHECK_GE(ranking.size(), fold);

  for (unsigned int i = 0; i < fold; ++i) {
    if (ranking[i] == "" && *next_unused != "")
      ranking[i] = *(next_unused++);
  }
}

std::vector<std::string> MaybeUpdateRankingFromHistory(
    const std::vector<std::string>& old_ranking,
    const std::map<std::string, int>& recent_share_history,
    const std::map<std::string, int>& all_share_history,
    unsigned int fold) {
  const double RECENCY_WEIGHT = 2.0;
  const double DAMPENING = 1.1;

  const std::string lowest_shown = old_ranking[fold - 1];
  const std::string highest_unshown_recent =
      HighestUnshown(old_ranking, recent_share_history, fold);
  const std::string highest_unshown_all =
      HighestUnshown(old_ranking, all_share_history, fold);

  std::vector<std::string> new_ranking = old_ranking;

  if (highest_unshown_recent != "" &&
      recent_share_history.at(highest_unshown_recent) * RECENCY_WEIGHT >
          recent_share_history.at(lowest_shown) * DAMPENING) {
    SwapRankingElement(new_ranking, lowest_shown, highest_unshown_recent);
  } else if (highest_unshown_all != "" &&
             all_share_history.at(highest_unshown_all) >
                 all_share_history.at(lowest_shown) * DAMPENING) {
    SwapRankingElement(new_ranking, lowest_shown, highest_unshown_all);
  }

  DCHECK_EQ(old_ranking.size(), new_ranking.size());
  return new_ranking;
}

#if DCHECK_IS_ON()
bool EveryElementInList(const std::vector<std::string>& ranking,
                        const std::vector<std::string>& available) {
  for (const auto& e : ranking) {
    if (!RankingContains(available, e))
      return false;
  }
  return true;
}

bool ElementIndexesAreUnchanged(const std::vector<std::string>& display,
                                const std::vector<std::string>& old,
                                unsigned int fold) {
  for (unsigned int i = 0; i < display.size() && i < fold; ++i) {
    if (RankingContains(old, display[i], fold) && display[i] != old[i])
      return false;
  }
  return true;
}

bool AtMostOneSlotChanged(const std::vector<std::string>& old_ranking,
                          const std::vector<std::string>& new_ranking,
                          unsigned int fold) {
  bool change_seen = false;
  for (unsigned int i = 0; i < old_ranking.size() && i < fold; ++i) {
    if (old_ranking[i] != new_ranking[i]) {
      if (change_seen)
        return false;
      else
        change_seen = true;
    }
  }
  return true;
}

bool NoEmptySlots(const std::vector<std::string>& display_ranking,
                  unsigned int fold) {
  if (display_ranking.size() < fold)
    return false;
  for (unsigned int i = 0; i < fold; i++) {
    if (display_ranking[i] == "")
      return false;
  }
  return true;
}
#endif  // DCHECK_IS_ON()

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
                        unsigned int fold,
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
    unsigned int fold,
    bool fix_more,
    Ranking* display_ranking,
    Ranking* persisted_ranking) {
  // Preconditions:
  DCHECK_GE(old_ranking.size(), fold);
  DCHECK_GE(available_on_system.size(), fold);

  std::vector<std::string> new_ranking = MaybeUpdateRankingFromHistory(
      old_ranking, all_share_history, recent_share_history, fold);

  Ranking computed_display_ranking =
      ReplaceUnavailableEntries(new_ranking, available_on_system);

  FillGaps(computed_display_ranking, available_on_system, fold);

  computed_display_ranking.resize(fold);

  if (fix_more)
    computed_display_ranking[fold - 1] = kMoreTarget;

  *persisted_ranking = new_ranking;
  *display_ranking = computed_display_ranking;

  // Postconditions:
#if DCHECK_IS_ON()
  {
    std::vector<std::string> available = available_on_system;
    available.push_back(kMoreTarget);

    DCHECK(EveryElementInList(*display_ranking, available));
    DCHECK(ElementIndexesAreUnchanged(*display_ranking, old_ranking, fold));
    DCHECK(AtMostOneSlotChanged(old_ranking, *persisted_ranking, fold));
    DCHECK(NoEmptySlots(*display_ranking, fold));
  }
#endif  // DCHECK_IS_ON()
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
