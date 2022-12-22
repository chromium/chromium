// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_USER_SCRIPT_LISTENER_H_
#define CHROME_BROWSER_EXTENSIONS_USER_SCRIPT_LISTENER_H_

#include <list>
#include <map>

#include "base/containers/circular_deque.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"

class GURL;
class URLPattern;
class ProfileManager;

namespace content {
class BrowserContext;
class NavigationHandle;
class NavigationThrottle;
}

namespace extensions {
class Extension;

// This class handles delaying of resource loads that depend on unloaded user
// scripts. For each request that comes in, we check if its url pattern matches
// one that user scripts will be injected into. If at least one matching user
// script has not been loaded yet, then we delay the request.
//
// This class lives on the UI thread.
class UserScriptListener : public ExtensionRegistryObserver,
                           public ProfileManagerObserver {
 public:
  UserScriptListener();

  UserScriptListener(const UserScriptListener&) = delete;
  UserScriptListener& operator=(const UserScriptListener&) = delete;

  ~UserScriptListener() override;

  // Constructs a NavigationThrottle if the UserScriptListener needs to delay
  // the given navigation. Otherwise, this method returns NULL.
  std::unique_ptr<content::NavigationThrottle> CreateNavigationThrottle(
      content::NavigationHandle* navigation_handle);

  // Called when manifest scripts have finished loading for the given
  // BrowserContext.
  void OnScriptsLoaded(content::BrowserContext* context);

  // Called when the owning BrowserClient is notified that we should begin
  // releasing our resources.
  void StartTearDown();

  void SetUserScriptsNotReadyForTesting(content::BrowserContext* context);
  void TriggerUserScriptsReadyForTesting(content::BrowserContext* context);

 private:
  using URLPatterns = std::list<URLPattern>;

  bool ShouldDelayRequest(const GURL& url);
  void StartDelayedRequests();

  // Update user_scripts_ready_ based on the status of all profiles. On a
  // transition from false to true, we resume all delayed requests.
  void CheckIfAllUserScriptsReady();

  // Resume any requests that we delayed in order to wait for user scripts.
  void UserScriptsReady(content::BrowserContext* context);

  // Clean up per-profile information related to the given profile.
  void ProfileDestroyed(content::BrowserContext* context);

  // Appends new url patterns to our list, also setting user_scripts_ready_
  // to false.
  void AppendNewURLPatterns(content::BrowserContext* context,
                            const URLPatterns& new_patterns);

  // Replaces our url pattern list. This is only used when patterns have been
  // deleted, so user_scripts_ready_ remains unchanged.
  void ReplaceURLPatterns(content::BrowserContext* context,
                          const URLPatterns& patterns);

  // True if all user scripts from all profiles are ready.
  bool user_scripts_ready_ = false;

  // Stores a throttle per URL request that we have delayed.
  class Throttle;
  using WeakThrottle = base::WeakPtr<Throttle>;
  using WeakThrottleList = base::circular_deque<WeakThrottle>;
  WeakThrottleList throttles_;

  // Per-profile bookkeeping so we know when all user scripts are ready.
  struct ProfileData;
  using ProfileDataMap = std::map<content::BrowserContext*, ProfileData>;
  ProfileDataMap profile_data_;

  // --- UI thread:

  // Helper to collect the extension's user script URL patterns in a list and
  // return it.
  void CollectURLPatterns(content::BrowserContext* context,
                          const Extension* extension,
                          URLPatterns* patterns);

  // ProfileManagerObserver
  void OnProfileAdded(Profile* profile) override;

  // ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;
  void OnShutdown(ExtensionRegistry* registry) override;

  base::ScopedMultiSourceObservation<extensions::ExtensionRegistry,
                                     extensions::ExtensionRegistryObserver>
      extension_registry_observations_{this};

  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observation_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_USER_SCRIPT_LISTENER_H_
