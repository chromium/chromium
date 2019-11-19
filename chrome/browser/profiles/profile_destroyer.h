// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_DESTROYER_H_
#define CHROME_BROWSER_PROFILES_PROFILE_DESTROYER_H_

#include <stdint.h>

#include <set>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/timer/timer.h"
#include "content/public/browser/render_process_host_observer.h"

class Profile;

namespace content {
class RenderProcessHost;
}

// We use this class to destroy the off the record profile so that we can make
// sure it gets done asynchronously after all render process hosts are gone.
class ProfileDestroyer : public content::RenderProcessHostObserver {
 public:
  static void DestroyProfileWhenAppropriate(Profile* const profile);
  static void DestroyOffTheRecordProfileNow(Profile* const profile);

 private:
  typedef std::set<content::RenderProcessHost*> HostSet;
  typedef std::set<ProfileDestroyer*> DestroyerSet;

  friend class base::RefCounted<ProfileDestroyer>;

  ProfileDestroyer(Profile* const profile, HostSet* hosts);
  ~ProfileDestroyer() override;

  // content::RenderProcessHostObserver override.
  void RenderProcessHostDestroyed(content::RenderProcessHost* host) override;

  // Called by the timer to cancel the pending destruction and do it now.
  void DestroyProfile();

  // Fetch the list of render process hosts that still point to |profile_ptr|.
  // |profile_ptr| is a void* because the Profile object may be freed. Only
  // pointer comparison is allowed, it will never be dereferenced as a Profile.
  static HostSet GetHostsForProfile(void* const profile_ptr);

  // We need access to all pending destroyers so we can cancel them.
  static DestroyerSet* pending_destroyers_;

  // We don't want to wait forever, so we have a cancellation timer.
  base::OneShotTimer timer_;

  // Used to count down the number of render process host left.
  uint32_t num_hosts_;

  // The profile being destroyed. If it is set to NULL, it is a signal from
  // another instance of ProfileDestroyer that this instance is canceled.
  Profile* profile_;

  base::WeakPtrFactory<ProfileDestroyer> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ProfileDestroyer);
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_DESTROYER_H_
