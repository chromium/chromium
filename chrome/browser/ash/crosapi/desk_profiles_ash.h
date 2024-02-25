// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_DESK_PROFILES_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_DESK_PROFILES_ASH_H_

#include <vector>

#include "ash/public/cpp/desk_profiles_delegate.h"
#include "base/observer_list.h"
#include "chromeos/crosapi/mojom/desk_profiles.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace crosapi {

// Ash-Chrome implementation of `mojom::DeskProfileObserver`. Receives profile
// information from Lacros.
class DeskProfilesAsh : public mojom::DeskProfileObserver,
                        public ash::DeskProfilesDelegate {
 public:
  DeskProfilesAsh();
  DeskProfilesAsh(const DeskProfilesAsh&) = delete;
  DeskProfilesAsh& operator=(const DeskProfilesAsh&) = delete;
  ~DeskProfilesAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::DeskProfileObserver> receiver);

 private:
  // mojom::DeskProfileObserver:
  void OnProfileUpsert(
      std::vector<mojom::LacrosProfileSummaryPtr> profiles) override;
  void OnProfileRemoved(uint64_t profile_id) override;

  // ash::DeskProfilesDelegate:
  const std::vector<ash::LacrosProfileSummary>& GetProfilesSnapshot()
      const override;
  uint64_t GetPrimaryProfileId() const override;
  const ash::LacrosProfileSummary* GetProfilesSnapshotByProfileId(
      uint64_t profile_id) const override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  // Updates `profiles_` with a summary. If the profile is not known, it is
  // added. If it is known, it is updated. Returns a reference to the summary in
  // `profiles_`.
  ash::LacrosProfileSummary& UpsertProfile(ash::LacrosProfileSummary&& summary);

  // Removes the profile identified by `profile_id`. Returns true if the profile
  // was known.
  bool RemoveProfile(uint64_t profile_id);

  // Cached list of profiles received from Lacros.
  std::vector<ash::LacrosProfileSummary> profiles_;

  // The profile ID of the primary user.
  uint64_t primary_user_profile_id_ = 0;

  base::ObserverList<Observer> observers_;

  mojo::ReceiverSet<mojom::DeskProfileObserver> receivers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_DESK_PROFILES_ASH_H_
