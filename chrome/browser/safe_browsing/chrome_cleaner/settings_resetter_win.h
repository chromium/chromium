// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_SETTINGS_RESETTER_WIN_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_SETTINGS_RESETTER_WIN_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "chrome/browser/safe_browsing/settings_reset_prompt/default_settings_fetcher.h"

class Profile;
class ProfileResetter;

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace safe_browsing {

// Handles settings reset for user's profile to complete a Chrome Cleaner run.
// Allows tagging a profile for resetting once a cleanup starts and resetting
// settings once a cleanup is completed. Completed cleanup is identified by
// annotations in the registry written by the cleaner. Non-static members can
// only be called if PostCleanupSettingsResetter::IsEnabled() is true.
class PostCleanupSettingsResetter {
 public:
  class Delegate {
   public:
    Delegate();

    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;

    virtual ~Delegate();

    virtual void FetchDefaultSettings(
        DefaultSettingsFetcher::SettingsCallback callback);

    virtual std::unique_ptr<ProfileResetter> GetProfileResetter(
        Profile* profile);
  };

  PostCleanupSettingsResetter();

  PostCleanupSettingsResetter(const PostCleanupSettingsResetter&) = delete;
  PostCleanupSettingsResetter& operator=(const PostCleanupSettingsResetter&) =
      delete;

  virtual ~PostCleanupSettingsResetter();

  // Returns true if the in-browser cleaner UI is enabled.
  static bool IsEnabled();

  // Tags |profile| to have its settings reset once the current cleanup
  // finishes.
  void TagForResetting(Profile* profile);

  // Resets settings for the profiles in |profiles| that are tagged for
  // resetting if cleanup has completed. Invokes |done_callback| once all
  // profiles in |profiles| have been reset.
  void ResetTaggedProfiles(
      std::vector<Profile*> profiles,
      base::OnceClosure done_callback,
      std::unique_ptr<PostCleanupSettingsResetter::Delegate> delegate);

  // Registers the settings reset pending tracked preference.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 private:
  // This object doesn't hold any state, so it's safe to delete it even after
  // an async function is called. For example, it's fine to let the object get
  // out of scope after invoking ResetTaggedProfiles() and there is no need
  // to wait for the callback to be run to release it. If you are intending to
  // change that assumption, please make sure you don't break the contract
  // where this class is used.
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_SETTINGS_RESETTER_WIN_H_
