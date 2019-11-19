// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/user_script_listener.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/resource_type.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/shared_user_script_master.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/content_scripts_handler.h"
#include "extensions/common/url_pattern.h"

using content::NavigationThrottle;
using content::ResourceType;

namespace extensions {

class UserScriptListener::Throttle
    : public NavigationThrottle,
      public base::SupportsWeakPtr<UserScriptListener::Throttle> {
 public:
  explicit Throttle(content::NavigationHandle* navigation_handle)
      : NavigationThrottle(navigation_handle) {}

  void ResumeIfDeferred() {
    DCHECK(should_defer_);
    should_defer_ = false;
    // Only resume the request if |this| has deferred it.
    if (did_defer_) {
      UMA_HISTOGRAM_TIMES("Extensions.ThrottledNetworkRequestDelay",
                          timer_->Elapsed());
      Resume();
    }
  }

  // NavigationThrottle implementation:
  ThrottleCheckResult WillStartRequest() override {
    // Only defer requests if Resume has not yet been called.
    if (should_defer_) {
      did_defer_ = true;
      timer_.reset(new base::ElapsedTimer());
      return DEFER;
    }
    return PROCEED;
  }

  const char* GetNameForLogging() override {
    return "UserScriptListener::Throttle";
  }

 private:
  bool should_defer_ = true;
  bool did_defer_ = false;
  std::unique_ptr<base::ElapsedTimer> timer_;

  DISALLOW_COPY_AND_ASSIGN(Throttle);
};

struct UserScriptListener::ProfileData {
  // True if the user scripts contained in |url_patterns| are ready for
  // injection.
  bool user_scripts_ready = false;

  // A list of URL patterns that have will have user scripts applied to them.
  URLPatterns url_patterns;
};

UserScriptListener::UserScriptListener() {
  // Profile manager can be null in unit tests.
  if (g_browser_process->profile_manager()) {
    for (auto* profile :
         g_browser_process->profile_manager()->GetLoadedProfiles()) {
      extension_registry_observer_.Add(ExtensionRegistry::Get(profile));
    }
  }

  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_ADDED,
                 content::NotificationService::AllSources());
}

std::unique_ptr<NavigationThrottle>
UserScriptListener::CreateNavigationThrottle(
    content::NavigationHandle* navigation_handle) {
  if (!ShouldDelayRequest(navigation_handle->GetURL()))
    return nullptr;

  auto throttle = std::make_unique<Throttle>(navigation_handle);
  throttles_.push_back(throttle->AsWeakPtr());
  return throttle;
}

void UserScriptListener::SetUserScriptsNotReadyForTesting(
    content::BrowserContext* context) {
  AppendNewURLPatterns(context, {URLPattern(URLPattern::SCHEME_ALL,
                                            URLPattern::kAllUrlsPattern)});
}

void UserScriptListener::TriggerUserScriptsReadyForTesting(
    content::BrowserContext* context) {
  UserScriptsReady(context);
}

UserScriptListener::~UserScriptListener() {}

bool UserScriptListener::ShouldDelayRequest(const GURL& url) {
  // Note: we could delay only requests made by the profile who is causing the
  // delay, but it's a little more complicated to associate requests with the
  // right profile. Since this is a rare case, we'll just take the easy way
  // out.
  if (user_scripts_ready_)
    return false;

  for (ProfileDataMap::const_iterator pt = profile_data_.begin();
       pt != profile_data_.end(); ++pt) {
    for (auto it = pt->second.url_patterns.begin();
         it != pt->second.url_patterns.end(); ++it) {
      if ((*it).MatchesURL(url)) {
        // One of the user scripts wants to inject into this request, but the
        // script isn't ready yet. Delay the request.
        return true;
      }
    }
  }

  return false;
}

void UserScriptListener::StartDelayedRequests() {
  WeakThrottleList::const_iterator it;
  for (it = throttles_.begin(); it != throttles_.end(); ++it) {
    if (it->get())
      (*it)->ResumeIfDeferred();
  }
  throttles_.clear();
}

void UserScriptListener::CheckIfAllUserScriptsReady() {
  bool was_ready = user_scripts_ready_;

  user_scripts_ready_ = true;
  for (ProfileDataMap::const_iterator it = profile_data_.begin();
       it != profile_data_.end(); ++it) {
    if (!it->second.user_scripts_ready)
      user_scripts_ready_ = false;
  }

  if (user_scripts_ready_ && !was_ready)
    StartDelayedRequests();
}

void UserScriptListener::UserScriptsReady(content::BrowserContext* context) {
  DCHECK(!context->IsOffTheRecord());

  profile_data_[context].user_scripts_ready = true;
  CheckIfAllUserScriptsReady();
}

void UserScriptListener::AppendNewURLPatterns(content::BrowserContext* context,
                                              const URLPatterns& new_patterns) {
  DCHECK(!context->IsOffTheRecord());

  user_scripts_ready_ = false;

  ProfileData& data = profile_data_[context];
  data.user_scripts_ready = false;

  data.url_patterns.insert(data.url_patterns.end(),
                           new_patterns.begin(), new_patterns.end());
}

void UserScriptListener::ReplaceURLPatterns(content::BrowserContext* context,
                                            const URLPatterns& patterns) {
  DCHECK_EQ(1U, profile_data_.count(context));
  profile_data_[context].url_patterns = patterns;
}

void UserScriptListener::CollectURLPatterns(const Extension* extension,
                                            URLPatterns* patterns) {
  for (const std::unique_ptr<UserScript>& script :
       ContentScriptsInfo::GetContentScripts(extension)) {
    patterns->insert(patterns->end(), script->url_patterns().begin(),
                     script->url_patterns().end());
  }
}

void UserScriptListener::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_PROFILE_ADDED: {
      Profile* profile = content::Source<Profile>(source).ptr();
      auto* registry = ExtensionRegistry::Get(profile);
      DCHECK(!extension_registry_observer_.IsObserving(registry));
      extension_registry_observer_.Add(registry);

      SharedUserScriptMaster* user_script_master =
          ExtensionSystem::Get(profile)->shared_user_script_master();
      // Note: |user_script_master| can be null in some tests.
      if (user_script_master) {
        UserScriptLoader* loader = user_script_master->script_loader();
        DCHECK(!user_script_loader_observer_.IsObserving(loader));
        user_script_loader_observer_.Add(loader);
      }
      break;
    }
    default:
      NOTREACHED();
  }
}

void UserScriptListener::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  if (ContentScriptsInfo::GetContentScripts(extension).empty())
    return;  // no new patterns from this extension.

  URLPatterns new_patterns;
  CollectURLPatterns(extension, &new_patterns);
  if (!new_patterns.empty()) {
    AppendNewURLPatterns(browser_context, new_patterns);
  }
}

void UserScriptListener::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  if (ContentScriptsInfo::GetContentScripts(extension).empty())
    return;  // No patterns to delete for this extension.

  // Clear all our patterns and reregister all the still-loaded extensions.
  const ExtensionSet& extensions =
      ExtensionRegistry::Get(browser_context)->enabled_extensions();
  URLPatterns new_patterns;
  for (ExtensionSet::const_iterator it = extensions.begin();
       it != extensions.end(); ++it) {
    if (it->get() != extension)
      CollectURLPatterns(it->get(), &new_patterns);
  }
  ReplaceURLPatterns(browser_context, new_patterns);
}

void UserScriptListener::OnShutdown(ExtensionRegistry* registry) {
  extension_registry_observer_.Remove(registry);
}

void UserScriptListener::OnScriptsLoaded(
    UserScriptLoader* loader,
    content::BrowserContext* browser_context) {
  UserScriptsReady(browser_context);
}

void UserScriptListener::OnUserScriptLoaderDestroyed(UserScriptLoader* loader) {
  user_script_loader_observer_.Remove(loader);
}

}  // namespace extensions
