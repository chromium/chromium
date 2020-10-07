// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_CONTEXT_H_
#define CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_CONTEXT_H_

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

 private:
  friend class BorealisContextManagerImpl;

  BorealisContext() = default;
  explicit BorealisContext(Profile* profile) : profile_(profile) {}

  Profile* profile_ = nullptr;
  bool borealis_running_ = false;
};

}  // namespace borealis

#endif  // CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_CONTEXT_H_
