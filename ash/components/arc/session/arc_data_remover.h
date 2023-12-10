// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_SESSION_ARC_DATA_REMOVER_H_
#define ASH_COMPONENTS_ARC_SESSION_ARC_DATA_REMOVER_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "components/prefs/pref_member.h"

class PrefService;

namespace arc {

// Manages ARC's user data removal operation.
class ArcDataRemover {
 public:
  ArcDataRemover(PrefService* prefs,
                 const cryptohome::Identification& cryptohome_id);

  ArcDataRemover(const ArcDataRemover&) = delete;
  ArcDataRemover& operator=(const ArcDataRemover&) = delete;

  ~ArcDataRemover();

  // Schedules to remove the data. This is persistent, calling Run() just
  // after rebooting may execute the removing.
  void Schedule();

  // Returns whether data removal is scheduled or not for testing purpose.
  bool IsScheduledForTesting() const;

  // Executes the removing, if scheduled.
  // This must run while ARC instance is stopped.
  // If not scheduled, |callback| will be synchronously called with nullopt.
  using RunCallback = base::OnceCallback<void(std::optional<bool> result)>;
  void Run(RunCallback callback);

 private:
  void OnDataRemoved(RunCallback callback, bool success);

  THREAD_CHECKER(thread_checker_);

  // Pref accessor to the "arc.data.remove_requested".
  BooleanPrefMember pref_;

  // Cryptohome ID for the user whose /data is being deleted.
  const cryptohome::Identification cryptohome_id_;

  base::WeakPtrFactory<ArcDataRemover> weak_factory_{this};
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_SESSION_ARC_DATA_REMOVER_H_
