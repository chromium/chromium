// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_BOREALIS_CONTEXT_H_
#define CHROME_BROWSER_ASH_BOREALIS_BOREALIS_CONTEXT_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/borealis/borealis_launch_options.h"

class Profile;

namespace guest_os {
class GuestOsStabilityMonitor;
}
namespace borealis {

class BorealisEngagementMetrics;
class BorealisLifetimeObserver;
class BorealisPowerController;

// An object to track information about the state of the Borealis VM.
// BorealisContext objects should only be created by the Borealis Context
// Manager, which is why the constructor is private.
class BorealisContext {
 public:
  BorealisContext(const BorealisContext&) = delete;
  BorealisContext& operator=(const BorealisContext&) = delete;
  ~BorealisContext();

  static std::unique_ptr<BorealisContext> CreateBorealisContextForTesting(
      Profile* profile);

  Profile* profile() const { return profile_; }

  const BorealisLaunchOptions::Options& launch_options() const {
    return launch_options_;
  }
  void set_launch_options(BorealisLaunchOptions::Options launch_options) {
    launch_options_ = std::move(launch_options);
  }

  const std::string& vm_name() const { return vm_name_; }
  void set_vm_name(std::string vm_name) { vm_name_ = std::move(vm_name); }

  const std::string& container_name() const { return container_name_; }
  void set_container_name(std::string container_name) {
    container_name_ = std::move(container_name);
  }

  const base::FilePath& disk_path() const { return disk_path_; }
  void set_disk_path(base::FilePath path) { disk_path_ = std::move(path); }

  // Called to signal that this Borealis VM is being unexpectedly shut down.
  // Not to be called during intentional shutdowns.
  void NotifyUnexpectedVmShutdown();

 private:
  friend class BorealisContextManagerImpl;

  explicit BorealisContext(Profile* profile);

  const raw_ptr<Profile> profile_;
  BorealisLaunchOptions::Options launch_options_;
  std::string vm_name_;
  std::string container_name_;
  base::FilePath disk_path_;
  // This instance listens for the session to finish and issues an automatic
  // shutdown when it does.
  std::unique_ptr<BorealisLifetimeObserver> lifetime_observer_;

  std::unique_ptr<guest_os::GuestOsStabilityMonitor>
      guest_os_stability_monitor_;

  std::unique_ptr<BorealisEngagementMetrics> engagement_metrics_;

  std::unique_ptr<BorealisPowerController> power_controller_;
};

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_BOREALIS_CONTEXT_H_
