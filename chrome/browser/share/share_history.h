// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARE_SHARE_HISTORY_H_
#define CHROME_BROWSER_SHARE_SHARE_HISTORY_H_

#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "build/build_config.h"
#include "chrome/browser/share/proto/share_history_message.pb.h"
#include "components/leveldb_proto/public/proto_database.h"

class Profile;

namespace sharing {

// A ShareHistory instance stores and provides access to the history of which
// targets the user has shared to. This history is stored per-profile and
// cleared when the user clears browsing data.
class ShareHistory : public base::SupportsUserData::Data {
 public:
  struct Target {
    std::string component_name;
    int count;
  };

  using BackingDb = leveldb_proto::ProtoDatabase<mojom::ShareHistory>;
  using GetFlatHistoryCallback = base::OnceCallback<void(std::vector<Target>)>;

  static void CreateForProfile(Profile* profile);
  static ShareHistory* Get(Profile* profile);

  explicit ShareHistory(Profile* profile,
                        std::unique_ptr<BackingDb> backing_db = nullptr);
  ~ShareHistory() override;

  virtual void AddShareEntry(const std::string& component_name);

  // Returns the flattened share history. Each entry in this list contains
  // the total count of shares the corresponding target has received over
  // the past |window| days. It is required that |window| <= the backend's
  // WINDOW value. A window of -1 means all available history.
  virtual void GetFlatShareHistory(GetFlatHistoryCallback callback,
                                   int window = -1);

  virtual void Clear(const base::Time& start = base::Time(),
                     const base::Time& end = base::Time());

  // Don't call this.
  //
  // TODO(ellyjones): There should be a better way to deal with this - it's used
  // to deal with the fact that ShareHistory's destruction order wrt
  // ShareRanking is undefined, so ShareHistory can get torn down while
  // ShareRanking has a pending async call to it, after the pending call's reply
  // has been posted but before the posted response has been run.
  base::WeakPtr<ShareHistory> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 protected:
  // Constructor for test fakes only - this constructor leaves this object in an
  // invalid state, so you had better override the public methods with their own
  // implementations that don't rely on the base ones!
  ShareHistory();

 private:
  void Init();
  void OnInitDone(leveldb_proto::Enums::InitStatus status);
  void OnInitialReadDone(bool ok, std::unique_ptr<mojom::ShareHistory> history);

  void FlushToBackingDb();

  // These two methods get or create entries in the in-memory protobuf; they do
  // not actually write back to the DB.
  mojom::DayShareHistory* DayShareHistoryForToday();
  mojom::TargetShareHistory* TargetShareHistoryByName(
      mojom::DayShareHistory* history,
      const std::string& target_name);

  bool init_finished_ = false;
  leveldb_proto::Enums::InitStatus db_init_status_;

  base::OnceClosureList post_init_callbacks_;

  std::unique_ptr<BackingDb> db_;

  // Cached copy of the data that resides on disk; initialization is complete
  // when this has first been loaded. After that point, any changes are synced
  // back to disk asynchronously, but the in-memory version is authoritative.
  mojom::ShareHistory history_;

  // Used so that callbacks supplied to the backing LevelDB won't call us back
  // if the Profile we're attached to is destroyed while we have a pending
  // callback. This *shouldn't* happen, since the LevelDB is *usually* attached
  // to the Profile too, but it seems like it might be possible anyway.
  base::WeakPtrFactory<ShareHistory> weak_factory_{this};
};

}  // namespace sharing

#endif  // CHROME_BROWSER_SHARE_SHARE_HISTORY_H_
