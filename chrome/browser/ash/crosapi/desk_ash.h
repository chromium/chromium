// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_DESK_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_DESK_ASH_H_

#include <cstdint>
#include <memory>
#include <string>

#include "base/uuid.h"
#include "chrome/browser/ui/ash/desks/desks_client.h"
#include "chromeos/crosapi/mojom/desk.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash {
class Desk;
}

namespace crosapi {

// Implement the crosapi interface for desk.
class DeskAsh : public mojom::Desk {
 public:
  DeskAsh();
  DeskAsh(const DeskAsh& desk) = delete;
  DeskAsh& operator=(const DeskAsh& desk) = delete;
  ~DeskAsh() override;
  void BindReceiver(mojo::PendingReceiver<mojom::Desk> pending_receiver);

  // mojom::Desk override.
  void LaunchEmptyDesk(const std::string& desk_name,
                       LaunchEmptyDeskCallback callback) override;
  void RemoveDesk(const base::Uuid& desk_uuid,
                  bool combine_desk,
                  std::optional<bool> allow_undo,
                  RemoveDeskCallback callback) override;
  void GetTemplateJson(const base::Uuid& uuid,
                       GetTemplateJsonCallback callback) override;
  void GetAllDesks(GetAllDesksCallback callback) override;

  void SaveActiveDesk(SaveActiveDeskCallback callback) override;

  void DeleteSavedDesk(const base::Uuid& uuid,
                       DeleteSavedDeskCallback callback) override;

  void RecallSavedDesk(const base::Uuid& uuid,
                       RecallSavedDeskCallback callback) override;

  void SetAllDesksProperty(int32_t app_restore_window_id,
                           bool all_desks,
                           SetAllDesksPropertyCallback callback) override;
  void GetSavedDesks(GetSavedDesksCallback callback) override;
  void GetActiveDesk(GetActiveDeskCallback callback) override;
  void SwitchDesk(const base::Uuid& uuid, SwitchDeskCallback callback) override;
  void GetDeskByID(const base::Uuid& uuid,
                   GetDeskByIDCallback callback) override;
  void AddDeskEventObserver(
      mojo::PendingRemote<crosapi::mojom::DeskEventObserver> observer) override;
  void NotifyDeskAdded(const base::Uuid& uuid, bool from_undo = false);
  void NotifyDeskRemoved(const base::Uuid& uuid);
  void NotifyDeskSwitched(const base::Uuid& current_id,
                          const base::Uuid& previous_id);

 private:
  // Returns the window pointer by app restore window Id.
  aura::Window* GetWindowByAppRestoreWindowId(aura::Window* window,
                                              int32_t app_restore_window_id);

  mojo::ReceiverSet<mojom::Desk> receivers_;

  // Cache remote clients that are currently consuming desk events.
  mojo::RemoteSet<mojom::DeskEventObserver> remote_desk_event_observers_;

  base::WeakPtrFactory<DeskAsh> weak_ptr_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_DESK_ASH_H_
