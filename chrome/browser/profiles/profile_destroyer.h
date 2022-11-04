// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_DESTROYER_H_
#define CHROME_BROWSER_PROFILES_PROFILE_DESTROYER_H_

#include <stdint.h>

#include <memory>
#include <set>

#include "base/memory/ref_counted.h"
#include "base/scoped_multi_source_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"

class Profile;

// We use this class to destroy the profiles so that we can make sure it gets
// done asynchronously after all render process hosts are gone.
class ProfileDestroyer : public content::RenderProcessHostObserver {
 public:
  // Destroys the given original profile either instantly, or after a short
  // delay waiting for dependent renderer process hosts to destroy.
  static void DestroyOriginalProfileWhenAppropriate(
      std::unique_ptr<Profile> profile);

  // Destroys the given off-the-record profile either instantly, or after a
  // short delay waiting for dependent renderer process hosts to destroy.
  // `profile` should not be used after this call.
  //
  // OTR profiles are owned by their parent profile - the parent profile is
  // responsible for actually destroying the object.
  static void DestroyOTRProfileWhenAppropriate(Profile* profile);

  // Similar to DestroyOTRProfileWhenAppropriate(), but with an explicit
  // timeout to wait for the renderer host to be destroyed.
  static void DestroyOTRProfileWhenAppropriateWithTimeout(
      Profile* profile,
      base::TimeDelta timeout);

  // Force destroy |profile| immediately without waiting for dependent renderer
  // process hosts to be destroyed.
  static void DestroyOTRProfileImmediately(Profile* profile);

  // Force destroy all the profiles pending deletion. This is called by the
  // ProfileManager during shutdown.
  static void DestroyPendingProfilesForShutdown();

  ProfileDestroyer(const ProfileDestroyer&) = delete;
  ProfileDestroyer& operator=(const ProfileDestroyer&) = delete;

 protected:
  using HostSet = std::set<content::RenderProcessHost*>;

  // Similar to DestroyOriginalProfileWhenAppropriate(), but with an explicit
  // timeout to wait for the renderer host to be destroyed.
  static void DestroyOriginalProfileWhenAppropriateWithTimeout(
      std::unique_ptr<Profile> profile,
      base::TimeDelta timeout);

  // Destroys an Original (non-off-the-record) profile immediately.
  static void DestroyOriginalProfileNow(std::unique_ptr<Profile> profile);

  // Destroys an OffTheRecord profile immediately and removes it from all
  // pending destroyers.
  static void DestroyOffTheRecordProfileNow(Profile* profile);

  // Fetch the list of render process hosts that still point to |profile_ptr|.
  // |profile_ptr| is a void* because the Profile object may be freed. Only
  // pointer comparison is allowed, it will never be dereferenced as a Profile.
  //
  // If |include_spare_rph| is true, include spare render process hosts in the
  // output.
  static void GetHostsForProfile(HostSet* out,
                                 void* profile_ptr,
                                 bool include_spare_rph = false);

  // Returns the profile destroyer that has |profile| as the underlying profile
  // and that is not prepared for destruction if any.  Returns nullptr if such
  // a profile destroyer does not exist..
  static ProfileDestroyer* GetPendingDestroyerForProfile(
      const Profile* profile);

  ProfileDestroyer(Profile* profile, base::TimeDelta timeout);
  ~ProfileDestroyer() override;

  const base::TimeDelta& timeout() const { return timeout_; }

  // Returns the underlying profile that should be destructed.
  virtual Profile* GetProfile() = 0;

  // Destroys the underlying profile.
  virtual void DoDestroyUnderlyingProfile() = 0;

  // Retries the destruction of the underlying profile using the same timeout.
  virtual void RetryDestroyUnderlyingProfile() = 0;

 private:
  // Starts monitoring the |hosts| and the timeout. If |hosts| is empty, then
  // this function will delete the profile now.
  void Start(const HostSet& hosts);

  // Returns true when this profile destroyer was scheduled for destruction;
  bool is_prepared_for_destruction() const {
    return is_prepared_for_destruction_;
  }

  // content::RenderProcessHostObserver override.
  void RenderProcessHostDestroyed(content::RenderProcessHost* host) override;

  // Two final state for the ProfileDestroyer instance:
  // - Timeout(): It still exists some RenderProcessHost. Force delete the
  //              profile.
  // - Retry():   Every observed RenderProcessHost have been deleted. We can try
  //              again destroying the profile.
  void Retry();
  void Timeout();

  // We don't want to wait forever, so we have a cancellation timer.
  base::OneShotTimer timer_;

  base::ScopedMultiSourceObservation<content::RenderProcessHost,
                                     content::RenderProcessHostObserver>
      observations_{this};

  // Force-destruction timeout.
  const base::TimeDelta timeout_;

  bool is_prepared_for_destruction_ = false;

  // The initial value of |profile_| stored as uint64_t for traces. It is useful
  // for use in the destructor, because at the end, |profile_| is nullptr.
  const uint64_t profile_ptr_;

  base::WeakPtrFactory<ProfileDestroyer> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_DESTROYER_H_
