// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/signin/merge_session_throttling_utils.h"

#include <stdint.h>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/login/signin/oauth2_login_manager.h"
#include "chrome/browser/chromeos/login/signin/oauth2_login_manager_factory.h"
#include "chrome/common/url_constants.h"
#include "components/google/core/common/google_util.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "url/gurl.h"

using content::BrowserThread;
using content::WebContents;

namespace merge_session_throttling_utils {

namespace {

const int64_t kMaxSessionRestoreTimeInSec = 60;

// The set of blocked profiles.
class ProfileSet : public std::set<Profile*> {
 public:
  ProfileSet() {}

  virtual ~ProfileSet() { DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_); }

  static ProfileSet* Get();

 private:
  friend struct ::base::LazyInstanceTraitsBase<ProfileSet>;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(ProfileSet);
};

// Set of all of profiles for which restore session is in progress.
// This static member is accessible only form UI thread.
base::LazyInstance<ProfileSet>::DestructorAtExit g_blocked_profiles =
    LAZY_INSTANCE_INITIALIZER;

ProfileSet* ProfileSet::Get() {
  return g_blocked_profiles.Pointer();
}

// Global counter that keeps the track of session merge status for all
// encountered profiles. This is used to determine if a throttle should
// even be even added to new requests. Value of 0 (initial) means that we
// probably have some profiles to restore, while 1 means that all known
// profiles are restored.
base::AtomicRefCount g_all_profiles_restored_(0);

}  // namespace

bool ShouldAttachNavigationThrottle() {
  return user_manager::UserManager::IsInitialized();
}

bool AreAllSessionMergedAlready() {
  return !g_all_profiles_restored_.IsZero();
}

void BlockProfile(Profile* profile) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Add a new profile to the list of those that we are currently blocking
  // blocking page loading for.
  if (ProfileSet::Get()->find(profile) == ProfileSet::Get()->end()) {
    DVLOG(1) << "Blocking profile " << profile;
    ProfileSet::Get()->insert(profile);

    // Since a new profile just got blocked, we can not assume that
    // all sessions are merged anymore.
    if (AreAllSessionMergedAlready()) {
      g_all_profiles_restored_.Decrement();
      DVLOG(1) << "Marking all sessions unmerged!";
    }
  }
}

void UnblockProfile(Profile* profile) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Have we blocked loading of pages for this this profile
  // before?
  DVLOG(1) << "Unblocking profile " << profile;
  ProfileSet::Get()->erase(profile);

  // Check if there is any other profile to block on.
  if (ProfileSet::Get()->size() == 0) {
    g_all_profiles_restored_.Increment();
    DVLOG(1) << "All profiles merged "
             << g_all_profiles_restored_.SubtleRefCountForDebug();
  }
}

bool ShouldDelayRequestForProfile(Profile* profile) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!user_manager::UserManager::Get()->IsUserLoggedIn()) {
    return false;
  } else if (!user_manager::UserManager::Get()
                  ->IsLoggedInAsUserWithGaiaAccount()) {
    // This is not a regular user session, let's remove the throttle
    // permanently.
    if (!AreAllSessionMergedAlready())
      g_all_profiles_restored_.Increment();

    return false;
  }

  if (!profile)
    return false;

  chromeos::OAuth2LoginManager* login_manager =
      chromeos::OAuth2LoginManagerFactory::GetInstance()->GetForProfile(
          profile);
  if (!login_manager)
    return false;

  switch (login_manager->state()) {
    case chromeos::OAuth2LoginManager::SESSION_RESTORE_NOT_STARTED:
      // The session restore for this profile hasn't even started yet. Don't
      // block for now.
      // In theory this should not happen since we should
      // kick off the session restore process for the newly added profile
      // before we attempt loading any page.
      if (user_manager::UserManager::Get()->IsLoggedInAsUserWithGaiaAccount() &&
          !user_manager::UserManager::Get()->IsLoggedInAsStub()) {
        LOG(WARNING) << "Loading content for a profile without "
                     << "session restore?";
      }
      return false;
    case chromeos::OAuth2LoginManager::SESSION_RESTORE_PREPARING:
    case chromeos::OAuth2LoginManager::SESSION_RESTORE_IN_PROGRESS: {
      // Check if the session restore has been going on for a while already.
      // If so, don't attempt to block page loading.
      if ((base::Time::Now() - login_manager->session_restore_start())
              .InSeconds() > kMaxSessionRestoreTimeInSec) {
        UnblockProfile(profile);
        return false;
      }

      // Add a new profile to the list of those that we are currently blocking
      // blocking page loading for.
      BlockProfile(profile);
      return true;
    }
    case chromeos::OAuth2LoginManager::SESSION_RESTORE_DONE:
    case chromeos::OAuth2LoginManager::SESSION_RESTORE_FAILED:
    case chromeos::OAuth2LoginManager::SESSION_RESTORE_CONNECTION_FAILED: {
      UnblockProfile(profile);
      return false;
    }
  }

  NOTREACHED();
  return false;
}

bool ShouldDelayRequestForWebContents(content::WebContents* web_contents) {
  content::BrowserContext* browser_context = web_contents->GetBrowserContext();
  if (!browser_context)
    return false;

  return ShouldDelayRequestForProfile(
      Profile::FromBrowserContext(browser_context));
}

bool ShouldDelayUrl(const GURL& url) {
  // If we are loading google properties while merge session is in progress,
  // we will show delayed loading page instead.
  return !content::GetNetworkConnectionTracker()->IsOffline() &&
         !AreAllSessionMergedAlready() &&
         google_util::IsGoogleHostname(url.host_piece(),
                                       google_util::ALLOW_SUBDOMAIN);
}

bool IsSessionRestorePending(Profile* profile) {
  if (!profile)
    return false;

  chromeos::OAuth2LoginManager* login_manager =
      chromeos::OAuth2LoginManagerFactory::GetInstance()->GetForProfile(
          profile);
  bool pending_session_restore = false;
  if (login_manager) {
    switch (login_manager->state()) {
      case chromeos::OAuth2LoginManager::SESSION_RESTORE_PREPARING:
      case chromeos::OAuth2LoginManager::SESSION_RESTORE_IN_PROGRESS:
        pending_session_restore = true;
        break;

      default:
        break;
    }
  }

  return pending_session_restore;
}

}  // namespace merge_session_throttling_utils
