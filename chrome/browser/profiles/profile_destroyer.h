// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_DESTROYER_H_
#define CHROME_BROWSER_PROFILES_PROFILE_DESTROYER_H_

#include <stdint.h>

#include <set>

#include "base/memory/ref_counted.h"
#include "base/scoped_multi_source_observation.h"
#include "base/timer/timer.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"

class DevToolsBrowserContextManager;
class Profile;
class ProfileImpl;

// We use this class to destroy the off the record profile so that we can make
// sure it gets done asynchronously after all render process hosts are gone.
class ProfileDestroyer : public content::RenderProcessHostObserver {
 public:
  // Destroys the given profile either instantly, or after a short delay waiting
  // for dependent renderer process hosts to destroy.
  // Ownership of the profile is passed to profile destroyer and the profile
  // should not be used after this call.
  static void DestroyProfileWhenAppropriate(Profile* profile);

  // Force destroy all the profiles pending deletion. This is called by the
  // ProfileManager during shutdown.
  static void DestroyPendingProfilesForShutdown();

  ProfileDestroyer(const ProfileDestroyer&) = delete;
  ProfileDestroyer& operator=(const ProfileDestroyer&) = delete;

 private:
  friend class ProfileImpl;
  friend class base::RefCounted<ProfileDestroyer>;

  // For custom timeout, see DestroyProfileWhenAppropriateWithTimeout.
  friend class DevToolsBrowserContextManager;

  using HostSet = std::set<content::RenderProcessHost*>;

  // Same as DestroyProfileWhenAppropriate, but configures how long to wait
  // for render process hosts to be destroyed. Intended for testing/automation
  // scenarios, where default timeout is too short.
  static void DestroyProfileWhenAppropriateWithTimeout(Profile* profile,
                                                       base::TimeDelta timeout);

  ProfileDestroyer(Profile* profile,
                   const HostSet& hosts,
                   base::TimeDelta timeout);
  ~ProfileDestroyer() override;

  // content::RenderProcessHostObserver override.
  void RenderProcessHostDestroyed(content::RenderProcessHost* host) override;

  // Two final state for the ProfileDestroyer instance:
  // - Timeout(): It still exists some RenderProcessHost. Force delete the
  //              profile.
  // - Retry():   Every observed RenderProcessHost have been deleted. We can try
  //              again destroying the profile.
  void Retry();
  void Timeout();

  // Fetch the list of render process hosts that still point to |profile_ptr|.
  // |profile_ptr| is a void* because the Profile object may be freed. Only
  // pointer comparison is allowed, it will never be dereferenced as a Profile.
  //
  // If |include_spare_rph| is true, include spare render process hosts in the
  // output.
  static void GetHostsForProfile(HostSet* out,
                                 void* profile_ptr,
                                 bool include_spare_rph = false);

  // Destroys a profile immediately.
  static void DestroyProfileNow(Profile* profile);

  // Destroys an Original (non-off-the-record) profile immediately.
  static void DestroyOriginalProfileNow(Profile* profile);

  // Destroys an OffTheRecord profile immediately and removes it from all
  // pending destroyers.
  static void DestroyOffTheRecordProfileNow(Profile* profile);

  // We don't want to wait forever, so we have a cancellation timer.
  base::OneShotTimer timer_;

  base::ScopedMultiSourceObservation<content::RenderProcessHost,
                                     content::RenderProcessHostObserver>
      observations_{this};

  // The profile being destroyed.
  //
  // Note: Ownership model of the Profile is not consistent. As a result, this
  // variable sometimes represent ownership over the Profile, but sometimes
  // this is just a weak reference, and the Profile might be destroyed outside
  // of the ProfileDestroyer.
  //
  // [Regular profile]
  // Owned by the ProfileManager. Ownership is transferred.
  //
  // [OTR profile]
  // Owned by the original profile. Owner is NOT transferred. This is a weak
  // pointer. Deleting the original Profile will delete its OTR profile under
  // the hood.
  //
  // [Independent profile]
  // It depends on the component. Most likely, the ownership is transferred.
  base::WeakPtr<Profile> profile_;

  // Force-destruction timeout.
  const base::TimeDelta timeout_;

  // The initial value of |profile_| stored as uint64_t for traces. It is useful
  // for use in the destructor, because at the end, |profile_| is nullptr.
  const uint64_t profile_ptr_;

  base::WeakPtrFactory<ProfileDestroyer> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_DESTROYER_H_
