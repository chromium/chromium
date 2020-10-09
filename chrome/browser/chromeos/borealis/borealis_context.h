// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_CONTEXT_H_
#define CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_CONTEXT_H_

#include <string>

#include "base/files/file_path.h"
#include "chrome/browser/profiles/profile.h"

namespace borealis {

// An object to track information about the state of the Borealis VM.
// BorealisContext objects should only be created by the Borealis Context
// Manager, which is why the constructor is private.
class BorealisContext {
 public:
  BorealisContext(const BorealisContext&) = delete;
  BorealisContext& operator=(const BorealisContext&) = delete;
  ~BorealisContext() = default;

  static BorealisContext* CreateBorealisContextForTesting() {
    return new BorealisContext();
  }

  Profile* profile() { return profile_; }
  void set_profile(Profile* profile) { profile_ = profile; }

  bool borealis_running() const { return borealis_running_; }
  void set_borealis_running(bool success) { borealis_running_ = success; }

  const std::string& container_name() { return container_name_; }

  const std::string& root_path() const { return root_path_; }
  void set_root_path(const std::string& path) { root_path_ = path; }

  const base::FilePath& disk_path() const { return disk_path_; }
  void set_disk_path(const base::FilePath& path) { disk_path_ = path; }

 private:
  friend class BorealisContextManagerImpl;

  BorealisContext();
  explicit BorealisContext(Profile* profile);

  Profile* profile_ = nullptr;
  bool borealis_running_ = false;
  std::string container_name_ = "borealis";
  std::string root_path_;
  base::FilePath disk_path_;
};

}  // namespace borealis

#endif  // CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_CONTEXT_H_
