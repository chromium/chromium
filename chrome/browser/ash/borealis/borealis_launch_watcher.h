// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_BOREALIS_LAUNCH_WATCHER_H_
#define CHROME_BROWSER_ASH_BOREALIS_BOREALIS_LAUNCH_WATCHER_H_

#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"

class Profile;

namespace borealis {

// Watches to see if a specified VM (from a given owner id and vm name) was
// started.
class BorealisLaunchWatcher : public ash::CiceroneClient::Observer {
 public:
  using OnLaunchCallback =
      base::OnceCallback<void(absl::optional<std::string>)>;

  BorealisLaunchWatcher(Profile* profile, std::string vm_name);
  BorealisLaunchWatcher(const BorealisLaunchWatcher&) = delete;
  BorealisLaunchWatcher() = delete;
  BorealisLaunchWatcher& operator=(const BorealisLaunchWatcher&) = delete;
  ~BorealisLaunchWatcher() override;

  // Adds a callback to be run when the VM is launched, the callback will be run
  // immediately if the VM has already been launched. If no event has been
  // observed after |timeout_|, then the callback is run anyway. If successful
  // the callback will be run with the name of the container that started,
  // otherwise it will be run with nullopt.
  void AwaitLaunch(OnLaunchCallback callback);

  void SetTimeoutForTesting(base::TimeDelta time) { timeout_ = time; }

 private:
  void TimeoutCallback();

  // CiceroneClient::Observer:
  void OnContainerStarted(
      const vm_tools::cicerone::ContainerStartedSignal& signal) override;

  std::string owner_id_;
  std::string vm_name_;
  base::TimeDelta timeout_ = base::Seconds(30);
  absl::optional<vm_tools::cicerone::ContainerStartedSignal>
      container_started_signal_;
  base::queue<OnLaunchCallback> callback_queue_;

  base::WeakPtrFactory<BorealisLaunchWatcher> weak_factory_{this};
};

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_BOREALIS_LAUNCH_WATCHER_H_
