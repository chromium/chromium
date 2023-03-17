// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_POWER_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_POWER_ASH_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chromeos/crosapi/mojom/power.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/device/public/mojom/wake_lock.mojom.h"

namespace base {
class SequencedTaskRunner;
class SingleThreadTaskRunner;
}  // namespace base

namespace device {
class PowerSaveBlocker;
}  // namespace device

namespace crosapi {

// The ash-chrome implementation of the Power crosapi interface.
class PowerAsh : public mojom::Power {
 public:
  PowerAsh();
  PowerAsh(const PowerAsh&) = delete;
  PowerAsh& operator=(const PowerAsh&) = delete;
  ~PowerAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::Power> receiver);

  // crosapi::mojom::Power:
  void AddPowerSaveBlocker(mojo::PendingRemote<mojom::PowerWakeLock> lock,
                           device::mojom::WakeLockType type,
                           device::mojom::WakeLockReason reason,
                           const std::string& description) override;
  void ReportActivity() override;

 private:
  // Handlers for disconnection of |lock| from AddPowerSaveBlocker(),
  void OnLockDisconnect(mojo::RemoteSetElementId id);

  // Support any number of connections.
  mojo::ReceiverSet<mojom::Power> receivers_;

  // Keeps track of |lock| passed by AddPowerSaveBlocker(), allowing
  // disconnection to be detected.
  mojo::RemoteSet<mojom::PowerWakeLock> lock_set_;

  // Storage of device::PowerSaveBlocker instances, keyed by
  // mojo::RemoteSetElementId in |lock_set_|.
  std::map<mojo::RemoteSetElementId, std::unique_ptr<device::PowerSaveBlocker>>
      power_save_blockers_;

  // Task runners needed by device::PowerSaveBlocker.
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner_;

  base::WeakPtrFactory<PowerAsh> weak_ptr_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_POWER_ASH_H_
