// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/share/share_history.h"

#include "base/containers/flat_map.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/share/proto/share_history_message.pb.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "content/public/browser/storage_partition.h"

namespace sharing {

namespace {

const char* const kShareHistoryFolder = "share_history";
static const char kShareHistoryKey[1] = "";

int TodaysDay() {
  return (base::Time::Now() - base::Time::UnixEpoch()).InDays();
}

std::unique_ptr<ShareHistory::BackingDb> MakeDefaultDbForProfile(
    Profile* profile) {
  return profile->GetDefaultStoragePartition()
      ->GetProtoDatabaseProvider()
      ->GetDB<mojom::ShareHistory>(
          leveldb_proto::ProtoDbType::SHARE_HISTORY_DATABASE,
          profile->GetPath().Append(kShareHistoryFolder),
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::TaskPriority::BEST_EFFORT}));
}

}  // namespace

// static
void ShareHistory::CreateForProfile(Profile* profile) {
  auto instance = std::make_unique<ShareHistory>(profile);
  profile->SetUserData(kShareHistoryKey, base::WrapUnique(instance.release()));
}

// static
ShareHistory* ShareHistory::Get(Profile* profile) {
  base::SupportsUserData::Data* instance =
      profile->GetUserData(kShareHistoryKey);
  DCHECK(instance);
  return static_cast<ShareHistory*>(instance);
}

ShareHistory::~ShareHistory() = default;

ShareHistory::ShareHistory(Profile* profile,
                           std::unique_ptr<BackingDb> backing_db)
    : db_(backing_db ? std::move(backing_db)
                     : MakeDefaultDbForProfile(profile)) {
  Init();
}

void ShareHistory::AddShareEntry(const std::string& component_name) {
  if (!init_finished_) {
    post_init_callbacks_.AddUnsafe(base::BindOnce(
        &ShareHistory::AddShareEntry, base::Unretained(this), component_name));
    return;
  }

  if (db_init_status_ != leveldb_proto::Enums::kOK)
    return;

  mojom::DayShareHistory* day_history = DayShareHistoryForToday();
  mojom::TargetShareHistory* target_history =
      TargetShareHistoryByName(day_history, component_name);

  target_history->set_count(target_history->count() + 1);

  // TODO(ellyjones): Start a write back to the backing database. Once that's
  // done, un-disable the AddsWrittenToBackingDb test.
}

void ShareHistory::GetFlatShareHistory(GetFlatHistoryCallback callback,
                                       int window) {
  if (!init_finished_) {
    post_init_callbacks_.AddUnsafe(base::BindOnce(
        &ShareHistory::GetFlatShareHistory, weak_factory_.GetWeakPtr(),
        std::move(callback), window));
    return;
  }

  if (db_init_status_ != leveldb_proto::Enums::kOK) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::vector<Target>()));
    return;
  }

  base::flat_map<std::string, int> counts;
  int today = TodaysDay();
  for (const auto& day : history_.day_histories()) {
    if (window != -1 && today - day.day() > window)
      continue;
    for (const auto& target : day.target_histories()) {
      counts[target.target().component_name()] += target.count();
    }
  }

  std::vector<Target> result;
  for (const auto& count : counts) {
    Target t;
    t.component_name = count.first;
    t.count = count.second;
    result.push_back(t);
  }

  std::sort(result.begin(), result.end(),
            [](const Target& a, const Target& b) { return a.count > b.count; });

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

void ShareHistory::Init() {
  db_->Init(
      base::BindOnce(&ShareHistory::OnInitDone, weak_factory_.GetWeakPtr()));
}

void ShareHistory::OnInitDone(leveldb_proto::Enums::InitStatus status) {
  init_finished_ = true;
  db_init_status_ = status;
  post_init_callbacks_.Notify();

  // TODO(ellyjones): Expire entries older than WINDOW days.
}

mojom::DayShareHistory* ShareHistory::DayShareHistoryForToday() {
  int today = TodaysDay();
  for (auto& day : *history_.mutable_day_histories()) {
    if (day.day() == today)
      return &day;
  }

  mojom::DayShareHistory* day = history_.mutable_day_histories()->Add();
  day->set_day(today);
  return day;
}

mojom::TargetShareHistory* ShareHistory::TargetShareHistoryByName(
    mojom::DayShareHistory* history,
    const std::string& target_name) {
  for (auto& target : *history->mutable_target_histories()) {
    if (target.target().component_name() == target_name)
      return &target;
  }

  mojom::TargetShareHistory* target =
      history->mutable_target_histories()->Add();
  target->mutable_target()->set_component_name(target_name);
  return target;
}

}  // namespace sharing
