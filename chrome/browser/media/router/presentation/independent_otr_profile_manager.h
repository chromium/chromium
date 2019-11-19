// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PRESENTATION_INDEPENDENT_OTR_PROFILE_MANAGER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PRESENTATION_INDEPENDENT_OTR_PROFILE_MANAGER_H_

#include <cstdint>
#include <memory>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/scoped_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/ui/browser_list_observer.h"

class Profile;

// This class manages Profile instances that were created by
// Profile::CreateOffTheRecordProfile().  These instances are owned by this
// class, instead of the original Profile (as is normally the case), as the
// lifetime assumptions are different.
//
// Normally, the last Browser object using an OTR profile will delete that
// profile (via its original Profile) when the Browser is closed.  Users of
// CreateOffTheRecordProfile can't let a Browser destroy an independently
// created OTR profile and must instead rely on this class.
//
// In particular, this functionality is used by extensions::OffscreenTab and
// PresentationReceiverWindowController (used by the Presentation API).  In the
// case of offscreen tabs, there is no interaction with a Browser object outside
// of DevTools.  The Presentation API reciever Windows don't necessarily have
// special lifetime requirements but they still can't use the same destruction
// path in ~Browser as the normal OTR profile.  DevTools presents a similar
// problem for presentation API windows as well.
//
// Normally, the caller should own the registration and delete it if the
// original profile is destroyed.  This will be signaled via the callback passed
// to CreateFromOriginalProfile if the registration is still alive when the
// original profile is being destroyed.
//
// All methods must be called on the UI thread.
class IndependentOTRProfileManager final : public BrowserListObserver,
                                           public ProfileObserver {
 public:
  class OTRProfileRegistration {
   public:
    ~OTRProfileRegistration();

    Profile* profile() const { return profile_; }

   private:
    friend class IndependentOTRProfileManager;

    OTRProfileRegistration(IndependentOTRProfileManager* manager,
                           Profile* profile);

    IndependentOTRProfileManager* const manager_;
    Profile* const profile_;

    DISALLOW_COPY_AND_ASSIGN(OTRProfileRegistration);
  };

  using ProfileDestroyedCallback = base::OnceCallback<void(Profile*)>;

  static IndependentOTRProfileManager* GetInstance();

  // Creates an OTR profile from |profile| and registers it with this object.
  // |callback| will be called if |profile| is being destroyed.  If |callback|
  // is called, the registration this method returns should be destroyed.
  std::unique_ptr<OTRProfileRegistration> CreateFromOriginalProfile(
      Profile* profile,
      ProfileDestroyedCallback callback);

 private:
  friend struct base::DefaultSingletonTraits<IndependentOTRProfileManager>;

  IndependentOTRProfileManager();
  ~IndependentOTRProfileManager() final;

  bool HasDependentProfiles(Profile* profile) const;
  void UnregisterProfile(Profile* profile);

  // chrome::BrowserListObserver:
  void OnBrowserAdded(Browser* browser) final;
  void OnBrowserRemoved(Browser* browser) final;

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) final;

  // Counts the number of Browser instances referencing an independent OTR
  // profile plus 1 for the OTRProfileRegistration object that created it.
  base::flat_map<Profile*, int32_t> refcounts_map_;
  base::flat_map<Profile*, ProfileDestroyedCallback> callbacks_map_;

  ScopedObserver<Profile, ProfileObserver> observed_original_profiles_{this};

  DISALLOW_COPY_AND_ASSIGN(IndependentOTRProfileManager);
};

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PRESENTATION_INDEPENDENT_OTR_PROFILE_MANAGER_H_
