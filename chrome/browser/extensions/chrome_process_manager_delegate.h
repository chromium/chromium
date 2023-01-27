// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CHROME_PROCESS_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_CHROME_PROCESS_MANAGER_DELEGATE_H_

#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "extensions/browser/process_manager_delegate.h"

class Browser;
class Profile;
class ProfileManager;

namespace extensions {

// Support for ProcessManager. Controls cases where Chrome wishes to disallow
// extension background pages or defer their creation.
class ChromeProcessManagerDelegate : public ProcessManagerDelegate,
                                     public BrowserListObserver,
                                     public ProfileManagerObserver,
                                     public ProfileObserver {
 public:
  ChromeProcessManagerDelegate();

  ChromeProcessManagerDelegate(const ChromeProcessManagerDelegate&) = delete;
  ChromeProcessManagerDelegate& operator=(const ChromeProcessManagerDelegate&) =
      delete;

  ~ChromeProcessManagerDelegate() override;

  // ProcessManagerDelegate:
  bool AreBackgroundPagesAllowedForContext(
      content::BrowserContext* context) const override;
  bool IsExtensionBackgroundPageAllowed(
      content::BrowserContext* context,
      const Extension& extension) const override;
  bool DeferCreatingStartupBackgroundHosts(
      content::BrowserContext* context) const override;

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override;
  void OnProfileManagerDestroying() override;

  // ProfileObserver:
  void OnOffTheRecordProfileCreated(Profile* off_the_record_profile) override;
  void OnProfileWillBeDestroyed(Profile* profile) override;

 private:
  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observation_{this};

  base::ScopedMultiSourceObservation<Profile, ProfileObserver>
      observed_profiles_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_CHROME_PROCESS_MANAGER_DELEGATE_H_
