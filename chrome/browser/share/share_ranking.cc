// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/share/share_ranking.h"

#include <vector>

#include "base/containers/to_vector.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/share/default_ranking.h"
#include "chrome/browser/share/share_history.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "content/public/browser/storage_partition.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/callback_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/locale_utils.h"

// Must come after other includes, because FromJniType() uses Profile.
#include "chrome/browser/share/jni_headers/ShareRankingBridge_jni.h"

using base::android::JavaParamRef;
#endif

namespace sharing {

const char* const ShareRanking::kMoreTarget = "$more";

namespace {

const char* const kShareRankingFolder = "share_ranking";
const char* const kShareRankingKey = "share_ranking";

// TODO(ellyjones): This should probably be a field trial.
const int kRecentWindowDays = 7;

std::unique_ptr<ShareRanking::BackingDb> MakeDefaultDbForProfile(
    Profile* profile) {
  return profile->GetDefaultStoragePartition()
      ->GetProtoDatabaseProvider()
      ->GetDB<proto::ShareRanking>(
          leveldb_proto::ProtoDbType::SHARE_RANKING_DATABASE,
          profile->GetPath().AppendASCII(kShareRankingFolder),
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::TaskPriority::BEST_EFFORT}));
}

bool RankingContains(const std::vector<std::string>& ranking,
                     const std::string& element,
                     size_t upto_index = SIZE_MAX) {
  for (size_t i = 0; i < ranking.size() && i < upto_index; ++i) {
    if (ranking[i] == element)
      return true;
  }
  return false;
}

std::vector<std::string> OrderByUses(const std::vector<std::string>& ranking,
                                     const std::map<std::string, int>& uses) {
  std::vector<std::string> ordered_ranking(ranking.begin(), ranking.end());
  std::sort(ordered_ranking.begin(), ordered_ranking.end(),
            [&](const std::string& a, const std::string& b) -> bool {
              int ua = uses.count(a) > 0 ? uses.at(a) : 0;
              int ub = uses.count(b) > 0 ? uses.at(b) : 0;
              return ua > ub;
            });
  return ordered_ranking;
}

std::string HighestUnshown(const std::vector<std::string>& ranking,
                           const std::map<std::string, int>& uses,
                           size_t length) {
  std::vector<std::string> unshown =
      base::ToVector(base::span(ranking).subspan(length));
  return !unshown.empty() ? OrderByUses(unshown, uses).front() : "";
}

std::string LowestShown(const std::vector<std::string>& ranking,
                        const std::map<std::string, int>& uses,
                        size_t length) {
  std::vector<std::string> shown =
      base::ToVector(base::span(ranking).first(length));
  return !shown.empty() ? OrderByUses(shown, uses).back() : "";
}

void SwapRankingElement(std::vector<std::string>& ranking,
                        const std::string& from,
                        const std::string& to) {
  auto from_loc = base::ranges::find(ranking, from);
  auto to_loc = base::ranges::find(ranking, to);

  CHECK(from_loc != ranking.end());
  CHECK(to_loc != ranking.end());

  *from_loc = to;
  *to_loc = from;
}

std::vector<std::string> ReplaceUnavailableEntries(
    const std::vector<std::string>& ranking,
    const std::vector<std::string>& available) {
  std::vector<std::string> result;
  base::ranges::transform(
      ranking, std::back_inserter(result), [&](const std::string& e) {
        return RankingContains(available, e) ? e : std::string();
      });
  return result;
}

void FillGaps(std::vector<std::string>& ranking,
              const std::vector<std::string>& available,
              size_t length) {
  // Take the tail of the ranking (the part that won't be shown on the screen),
  // remove items that aren't available on the system. These will be the first
  // apps used for empty slots.
  std::vector<std::string> unused_available =
      base::ToVector(base::span(ranking).subspan(length));
  std::erase_if(unused_available, [&](const std::string& e) {
    return !RankingContains(available, e);
  });

  // Now, append the rest of the system apps (those not already included) to
  // unused_available. These will be the apps that can handle the share type and
  // that are available on the system but not included in the old ranking at
  // all, so these are the lowest priority targets, because they are likely to
  // be things like "Bluetooth" which users almost never share to.
  for (const auto& app : available) {
    if (!RankingContains(unused_available, app))
      unused_available.push_back(app);
  }

  base::span<std::string> candidates(unused_available);

  for (size_t i = 0; i < length && !candidates.empty(); ++i) {
    std::string& candidate = candidates.front();
    if (ranking[i] == "" && candidate != "") {
      ranking[i] = std::move(candidate);
      candidates = candidates.subspan(1);
    }
  }
}

std::vector<std::string> MaybeUpdateRankingFromHistory(
    const std::vector<std::string>& old_ranking,
    const std::map<std::string, int>& recent_share_history,
    const std::map<std::string, int>& all_share_history,
    size_t length) {
  const double DAMPENING = 1.1;

  const std::string lowest_shown_recent =
      LowestShown(old_ranking, recent_share_history, length);
  const std::string lowest_shown_all =
      LowestShown(old_ranking, all_share_history, length);
  const std::string highest_unshown_recent =
      HighestUnshown(old_ranking, recent_share_history, length);
  const std::string highest_unshown_all =
      HighestUnshown(old_ranking, all_share_history, length);

  std::vector<std::string> new_ranking = old_ranking;

  auto recent_count_for = [&](const std::string& key) {
    return recent_share_history.count(key) > 0 ? recent_share_history.at(key)
                                               : 0;
  };

  auto all_count_for = [&](const std::string& key) {
    return all_share_history.count(key) > 0 ? all_share_history.at(key) : 0;
  };

  if (highest_unshown_recent != "" &&
      recent_count_for(highest_unshown_recent) >
          recent_count_for(lowest_shown_recent) * DAMPENING) {
    SwapRankingElement(new_ranking, lowest_shown_recent,
                       highest_unshown_recent);
  } else if (highest_unshown_all != "" &&
             all_count_for(highest_unshown_all) >
                 all_count_for(lowest_shown_all) * DAMPENING) {
    SwapRankingElement(new_ranking, lowest_shown_all, highest_unshown_all);
  }

  CHECK_EQ(old_ranking.size(), new_ranking.size());
  return new_ranking;
}

ShareRanking::Ranking AppendUpToLength(
    const ShareRanking::Ranking& ranking,
    const std::map<std::string, int>& history,
    size_t length) {
  std::vector<std::string> history_keys;
  for (const auto& it : history)
    history_keys.push_back(it.first);
  ShareRanking::Ranking all = OrderByUses(history_keys, history);
  ShareRanking::Ranking result = ranking;
  while (result.size() < length && !all.empty()) {
    if (!RankingContains(result, all.front())) {
      result.push_back(all.front());
      all.erase(all.begin());
    }
  }
  return result;
}

#if BUILDFLAG(IS_ANDROID)
void RunJniRankCallback(base::android::ScopedJavaGlobalRef<jobject> callback,
                        JNIEnv* env,
                        std::optional<ShareRanking::Ranking> ranking) {
  auto result = base::android::ToJavaArrayOfStrings(env, ranking.value());
  base::android::RunObjectCallbackAndroid(callback, result);
}
#endif

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
                                size_t length) {
  for (size_t i = 0; i < display.size() && i < length; ++i) {
    if (RankingContains(old, display[i], length) && display[i] != old[i])
      return false;
  }
  return true;
}

bool AtMostOneSlotChanged(const std::vector<std::string>& old_ranking,
                          const std::vector<std::string>& new_ranking,
                          size_t length) {
  bool change_seen = false;
  for (size_t i = 0; i < old_ranking.size() && i < length; ++i) {
    if (old_ranking[i] != new_ranking[i]) {
      if (change_seen)
        return false;
      else
        change_seen = true;
    }
  }
  return true;
}

#endif  // DCHECK_IS_ON()

std::map<std::string, int> BuildHistoryMap(
    const std::vector<ShareHistory::Target>& flat_history) {
  std::map<std::string, int> result;
  for (const auto& entry : flat_history)
    result[entry.component_name] += entry.count;
  return result;
}

std::vector<std::string> AddMissingItemsFromHistory(
    const std::vector<std::string>& existing,
    const std::map<std::string, int>& history) {
  std::vector<std::string> updated = existing;
  for (const auto& item : history) {
    if (!RankingContains(updated, item.first))
      updated.push_back(item.first);
  }
  return updated;
}

}  // namespace

ShareRanking* ShareRanking::Get(Profile* profile) {
  if (profile->IsOffTheRecord())
    return nullptr;

  base::SupportsUserData::Data* instance =
      profile->GetUserData(kShareRankingKey);
  if (!instance) {
    auto new_instance = std::make_unique<ShareRanking>(profile);
    instance = new_instance.get();
    profile->SetUserData(kShareRankingKey, std::move(new_instance));
  }
  return static_cast<ShareRanking*>(instance);
}

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
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  if (ranking_.contains(type)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  std::make_optional(ranking_[type])));
    return;
  }

  db_->GetEntry(type, base::BindOnce(&ShareRanking::OnBackingGetDone,
                                     weak_factory_.GetWeakPtr(), type,
                                     std::move(callback)));
}

void ShareRanking::Rank(ShareHistory* history,
                        const std::string& type,
                        const std::vector<std::string>& available_on_system,
                        size_t fold,
                        size_t length,
                        bool persist_update,
                        GetRankingCallback callback) {
  auto pending_call = std::make_unique<PendingRankCall>();
  pending_call->type = type;
  pending_call->available_on_system = available_on_system;
  pending_call->fold = fold;
  pending_call->length = length;
  pending_call->persist_update = persist_update;
  pending_call->callback = std::move(callback);
  pending_call->history_db = history->GetWeakPtr();

  history->GetFlatShareHistory(base::BindOnce(&ShareRanking::OnRankGetAllDone,
                                              weak_factory_.GetWeakPtr(),
                                              std::move(pending_call)));
}

void ShareRanking::Clear(const base::Time& start, const base::Time& end) {
  // Since the ranking doesn't contain timestamps (they wouldn't really make
  // sense), there's no way to remove only ranking data that came from the
  // specified deletion interval. Instead, we forget all the ranking data
  // altogether whenever we're clearing any interval - this almost always
  // over-deletes, unfortunately.
  ranking_.clear();
  db_->UpdateEntriesWithRemoveFilter(
      std::make_unique<BackingDb::KeyEntryVector>(),
      base::BindRepeating([](const std::string& key) -> bool { return true; }),
      base::DoNothing());
}

// static
void ShareRanking::ComputeRanking(
    const std::map<std::string, int>& all_share_history,
    const std::map<std::string, int>& recent_share_history,
    const Ranking& old_ranking,
    const std::vector<std::string>& available_on_system,
    size_t fold,
    size_t length,
    Ranking* display_ranking,
    Ranking* persisted_ranking) {
  // Preconditions:
  CHECK_GT(fold, 0u);
  CHECK_GT(length, 0u);
  CHECK_LE(fold, length);
  CHECK_GE(old_ranking.size(), length - 1);

  Ranking augmented_old_ranking = AddMissingItemsFromHistory(
      AddMissingItemsFromHistory(old_ranking, all_share_history),
      recent_share_history);

  // If the fold and the length are equal, fill up to length - 1 from history,
  // leaving the last slot for $more. If they aren't equal, the fold must be
  // lower, so fill all the way up to the fold. After that, the code right below
  // will fill the slots between fold and length - 1 with more entries, leaving
  // the length - 1 slot for $more.
  std::vector<std::string> new_ranking = MaybeUpdateRankingFromHistory(
      augmented_old_ranking, all_share_history, recent_share_history,
      fold < length ? fold : length - 1);

  if (fold != length) {
    new_ranking =
        AppendUpToLength(new_ranking, recent_share_history, length - 1);
    new_ranking = AppendUpToLength(new_ranking, all_share_history, length - 1);
  }

  Ranking computed_display_ranking =
      ReplaceUnavailableEntries(new_ranking, available_on_system);

  FillGaps(computed_display_ranking, available_on_system, length - 1);

  computed_display_ranking.resize(length);
  computed_display_ranking[length - 1] = kMoreTarget;

  *persisted_ranking = new_ranking;
  *display_ranking = computed_display_ranking;

  // Postconditions. These ones require a bunch of computation so they're
  // DCHECK-only.
#if DCHECK_IS_ON()
  {
    std::vector<std::string> available = available_on_system;
    available.push_back(kMoreTarget);

    DCHECK(EveryElementInList(*display_ranking, available));
    DCHECK(ElementIndexesAreUnchanged(*display_ranking, old_ranking, fold - 1));
    DCHECK(AtMostOneSlotChanged(old_ranking, *persisted_ranking, fold - 1));

    DCHECK(RankingContains(*display_ranking, kMoreTarget));
  }
#endif  // DCHECK_IS_ON()

  CHECK_EQ(display_ranking->size(), length);
  CHECK_GE(persisted_ranking->size(), length - 1);
}

ShareRanking::PendingRankCall::PendingRankCall() = default;
ShareRanking::PendingRankCall::~PendingRankCall() = default;

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
  if (!ok || db_init_status_ != leveldb_proto::Enums::kOK || !ranking) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  ranking_[key].clear();
  for (const auto& it : ranking->targets()) {
    ranking_[key].push_back(it);
  }

  std::move(callback).Run(std::make_optional(ranking_[key]));
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

void ShareRanking::OnRankGetAllDone(std::unique_ptr<PendingRankCall> pending,
                                    std::vector<ShareHistory::Target> history) {
  pending->all_history = history;
  if (pending->history_db) {
    auto history_db = pending->history_db;
    history_db->GetFlatShareHistory(
        base::BindOnce(&ShareRanking::OnRankGetRecentDone,
                       weak_factory_.GetWeakPtr(), std::move(pending)),
        kRecentWindowDays);
  } else {
    std::move(pending->callback).Run(std::nullopt);
  }
}
void ShareRanking::OnRankGetRecentDone(
    std::unique_ptr<PendingRankCall> pending,
    std::vector<ShareHistory::Target> history) {
  pending->recent_history = history;
  // Grab the type out of pending before std::move()ing from it.
  std::string type = pending->type;
  GetRanking(type,
             base::BindOnce(&ShareRanking::OnRankGetOldRankingDone,
                            weak_factory_.GetWeakPtr(), std::move(pending)));
}
void ShareRanking::OnRankGetOldRankingDone(
    std::unique_ptr<PendingRankCall> pending,
    std::optional<Ranking> ranking) {
  if (!ranking)
    ranking = GetDefaultInitialRankingForType(pending->type);

  Ranking display, persisted;
  ComputeRanking(BuildHistoryMap(pending->all_history),
                 BuildHistoryMap(pending->recent_history), *ranking,
                 pending->available_on_system, pending->fold, pending->length,
                 &display, &persisted);

  if (pending->persist_update)
    UpdateRanking(pending->type, persisted);

  std::move(pending->callback).Run(display);
}

ShareRanking::Ranking ShareRanking::GetDefaultInitialRankingForType(
    const std::string& type) {
#if BUILDFLAG(IS_ANDROID)
  // On Android, just use the app's default locale string - we don't have a pref
  // locale to consult regardless, and l10n_util::GetApplicationLocale can do
  // blocking disk IO (!) while it checks whether we have a string pack for the
  // various eligible locales. We don't care about strings here, so just go with
  // what the system is set to, and don't block on the UI thread.
  std::string locale = base::android::GetDefaultLocaleString();
#else
  std::string locale = l10n_util::GetApplicationLocale("", false);
#endif
  return initial_ranking_for_test_.value_or(
      DefaultRankingForLocaleAndType(locale, type));
}

}  // namespace sharing

#if BUILDFLAG(IS_ANDROID)

void JNI_ShareRankingBridge_Rank(JNIEnv* env,
                                 Profile* profile,
                                 std::string& type,
                                 std::vector<std::string>& available,
                                 jint jfold,
                                 jint jlength,
                                 jboolean jpersist,
                                 const JavaParamRef<jobject>& jcallback) {
  base::android::ScopedJavaGlobalRef<jobject> callback(jcallback);

  if (profile->IsOffTheRecord()) {
    // For incognito/guest profiles, we use the source ranking from the parent
    // normal profile but never write anything back to that profile, meaning the
    // user will get their existing ranking but no change to it will be made
    // based on incognito activity.
    CHECK(!jpersist);
    profile = profile->GetOriginalProfile();
  }

  auto* history = sharing::ShareHistory::Get(profile);
  auto* ranking = sharing::ShareRanking::Get(profile);

  CHECK(history);
  CHECK(ranking);

  ranking->Rank(
      history, type, available, jfold, jlength, jpersist,
      base::BindOnce(&sharing::RunJniRankCallback, std::move(callback),
                     // TODO(ellyjones): Is it safe to unretained env here?
                     base::Unretained(env)));
}

#endif  // BUILDFLAG(IS_ANDROID)
