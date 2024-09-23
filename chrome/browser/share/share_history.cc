// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/share/share_history.h"

#include "base/containers/flat_map.h"
#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/share/proto/share_history_message.pb.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "content/public/browser/storage_partition.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_string.h"
#include "chrome/browser/profiles/profile.h"

// Must come after other includes, because FromJniType() uses Profile.
#include "chrome/browser/share/jni_headers/ShareHistoryBridge_jni.h"

using base::android::JavaParamRef;
#endif

namespace sharing {

namespace {

const char* const kShareHistoryFolder = "share_history";

// This is the key used for the single entry in the backing LevelDB. The fact
// that it is the same string as the above folder name is a coincidence; please
// do not fold these constants together.
const char* const kShareHistoryKey = "share_history";

constexpr auto kMaxHistoryAge = base::Days(90);

int TodaysDay() {
  return (base::Time::Now() - base::Time::UnixEpoch()).InDays();
}

std::unique_ptr<ShareHistory::BackingDb> MakeDefaultDbForProfile(
    Profile* profile) {
  return profile->GetDefaultStoragePartition()
      ->GetProtoDatabaseProvider()
      ->GetDB<mojom::ShareHistory>(
          leveldb_proto::ProtoDbType::SHARE_HISTORY_DATABASE,
          profile->GetPath().AppendASCII(kShareHistoryFolder),
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::TaskPriority::BEST_EFFORT}));
}

bool DayOverlapsTimeRange(base::Time day_start,
                          base::Time start,
                          base::Time end) {
  const base::Time epoch = base::Time::UnixEpoch();
  int day = (day_start - epoch).InDays();
  int start_day = (start - epoch).InDays();
  int end_day = (end - epoch).InDays();
  return day >= start_day && day <= end_day;
}

}  // namespace

// static
void ShareHistory::CreateForProfile(Profile* profile) {
  CHECK(!profile->IsOffTheRecord());
  auto instance = std::make_unique<ShareHistory>(profile);
  profile->SetUserData(kShareHistoryKey, base::WrapUnique(instance.release()));
}

// static
ShareHistory* ShareHistory::Get(Profile* profile) {
  if (profile->IsOffTheRecord())
    return nullptr;

  base::SupportsUserData::Data* instance =
      profile->GetUserData(kShareHistoryKey);
  if (!instance) {
    CreateForProfile(profile);
    instance = profile->GetUserData(kShareHistoryKey);
  }
  return static_cast<ShareHistory*>(instance);
}

ShareHistory::~ShareHistory() = default;

ShareHistory::ShareHistory() = default;
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

  FlushToBackingDb();
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
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
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

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

void ShareHistory::Clear(const base::Time& start, const base::Time& end) {
  google::protobuf::RepeatedPtrField<mojom::DayShareHistory> histories_to_keep;
  for (const auto& day : history_.day_histories()) {
    base::Time day_start = base::Time::UnixEpoch() + base::Days(day.day());
    if (!DayOverlapsTimeRange(day_start, start, end)) {
      mojom::DayShareHistory this_day;
      this_day.CopyFrom(day);
      histories_to_keep.Add(std::move(this_day));
    }
  }

  history_.mutable_day_histories()->Swap(&histories_to_keep);

  FlushToBackingDb();
}

void ShareHistory::Init() {
  db_->Init(
      base::BindOnce(&ShareHistory::OnInitDone, weak_factory_.GetWeakPtr()));
}

void ShareHistory::OnInitDone(leveldb_proto::Enums::InitStatus status) {
  db_init_status_ = status;
  if (status != leveldb_proto::Enums::kOK) {
    // If the LevelDB initialization failed, follow the same state transitions
    // as in the happy case, but without going through LevelDB; i.e., act as
    // though the initial read failed, instead of the LevelDB initialization, so
    // that control always ends up in OnInitialReadDone.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&ShareHistory::OnInitialReadDone,
                                  weak_factory_.GetWeakPtr(), false,
                                  std::make_unique<mojom::ShareHistory>()));
    return;
  }

  db_->GetEntry(kShareHistoryKey,
                base::BindOnce(&ShareHistory::OnInitialReadDone,
                               weak_factory_.GetWeakPtr()));
}

void ShareHistory::OnInitialReadDone(
    bool ok,
    std::unique_ptr<mojom::ShareHistory> history) {
  if (ok && history)
    history_ = *history;
  init_finished_ = true;
  post_init_callbacks_.Notify();

  Clear(base::Time(), base::Time::Now() - kMaxHistoryAge);
}

void ShareHistory::FlushToBackingDb() {
  auto keyvals = std::make_unique<BackingDb::KeyEntryVector>();
  keyvals->push_back({kShareHistoryKey, history_});

  db_->UpdateEntries(std::move(keyvals),
                     std::make_unique<std::vector<std::string>>(),
                     base::DoNothing());
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

#if BUILDFLAG(IS_ANDROID)
void JNI_ShareHistoryBridge_AddShareEntry(JNIEnv* env,
                                          Profile* profile,
                                          const JavaParamRef<jstring>& name) {
  auto* instance = sharing::ShareHistory::Get(profile);
  if (instance)
    instance->AddShareEntry(base::android::ConvertJavaStringToUTF8(env, name));
}

void JNI_ShareHistoryBridge_Clear(JNIEnv* env, Profile* profile) {
  auto* instance = sharing::ShareHistory::Get(profile);
  if (instance)
    instance->Clear();
}
#endif
