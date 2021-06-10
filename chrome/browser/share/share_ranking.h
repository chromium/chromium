// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARE_SHARE_RANKING_H_
#define CHROME_BROWSER_SHARE_SHARE_RANKING_H_

#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "chrome/browser/share/proto/share_ranking_message.pb.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace sharing {

class ShareHistory;

class ShareRanking : public base::SupportsUserData::Data {
 public:
  using Ranking = std::vector<std::string>;

  using BackingDb = leveldb_proto::ProtoDatabase<proto::ShareRanking>;
  using GetRankingCallback =
      base::OnceCallback<void(absl::optional<Ranking> result)>;

  explicit ShareRanking(Profile* profile,
                        std::unique_ptr<BackingDb> backing_db = nullptr);
  ~ShareRanking() override;

  void UpdateRanking(const std::string& type, Ranking ranking);
  void GetRanking(const std::string& type, GetRankingCallback callback);

  // This method:
  // 1. Fetches the existing rankings and histories for |type| from the provided
  //    databases
  // 2. Runs the ranking algorithm (see below).
  // 3. If |persist_update|, writes the new ranking back to |ranking|
  // 4. Returns the display ranking to use for this specific share
  void Rank(ShareHistory* history,
            const std::string& type,
            const std::vector<std::string>& available_on_system,
            int fold,
            bool persist_update,
            GetRankingCallback callback);

  // The core of the ranking algorithm, exposed as a pure function for ease of
  // testing and reasoning about the code. This function takes the existing
  // share history and ranking for this type, a set of all targets available on
  // the current system, and a fold, and computes the new display ranking and
  // the new persistent ranking.
  static void ComputeRanking(
      const std::map<std::string, int>& all_share_history,
      const std::map<std::string, int>& recent_share_history,
      const Ranking& old_ranking,
      const std::vector<std::string>& available_on_system,
      int fold,
      Ranking* display_ranking,
      Ranking* persisted_ranking);

 private:
  void Init();
  void OnInitDone(leveldb_proto::Enums::InitStatus status);
  void OnBackingGetDone(std::string key,
                        GetRankingCallback callback,
                        bool ok,
                        std::unique_ptr<proto::ShareRanking> ranking);

  void FlushToBackingDb(const std::string& key);

  bool init_finished_ = false;
  leveldb_proto::Enums::InitStatus db_init_status_;

  base::OnceClosureList post_init_callbacks_;

  std::unique_ptr<BackingDb> db_;

  base::flat_map<std::string, Ranking> ranking_;

  base::WeakPtrFactory<ShareRanking> weak_factory_{this};
};

}  // namespace sharing

#endif  // CHROME_BROWSER_SHARE_SHARE_RANKING_H_
