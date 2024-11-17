// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"

#include "base/debug/crash_logging.h"
#include "base/feature_list.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace {

std::string GetOriginName(ProfileKeepAliveOrigin origin) {
  std::ostringstream oss;
  oss << origin;
  return oss.str();
}

}  // namespace

ScopedProfileKeepAlive::ScopedProfileKeepAlive(const Profile* profile,
                                               ProfileKeepAliveOrigin origin)
    : profile_(profile->GetWeakPtr()), origin_(origin) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile_);
  // |profile_manager| can be nullptr in tests.
  auto* profile_manager = g_browser_process->profile_manager();
  if (profile_manager) {
    profile_manager->AddKeepAlive(profile_.get(), origin_);
  } else {
    // TODO(crbug.com/368360956): Not incrementing the refcount will cause
    // `profile` to get destroyed too early. Remove or convert to a CHECK() once
    // the root cause is fixed.
    SCOPED_CRASH_KEY_STRING32("ProfileKeepAlive", "origin",
                              GetOriginName(origin));
    base::debug::DumpWithoutCrashing();
  }
}

ScopedProfileKeepAlive::~ScopedProfileKeepAlive() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!base::FeatureList::IsEnabled(features::kDestroyProfileOnBrowserClose)) {
    // Decrementing the refcount won't cause Profile destruction in this
    // configuration, so we don't need to post a task.
    RemoveKeepAliveOnUIThread(profile_, origin_);
  } else {
    // The object that owns ScopedProfileKeepAlive might be owned by Profile,
    // in which case triggering ~Profile() from here causes UAF bugs. Post
    // RemoveKeepAlive() to a task to avoid this.
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&ScopedProfileKeepAlive::RemoveKeepAliveOnUIThread,
                       profile_, origin_));
  }
}

// static
void ScopedProfileKeepAlive::RemoveKeepAliveOnUIThread(
    base::WeakPtr<const Profile> profile,
    ProfileKeepAliveOrigin origin) {
  SCOPED_CRASH_KEY_STRING32("ProfileKeepAlive", "origin",
                            GetOriginName(origin));
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // |g_browser_process| could be nullptr if this is called during shutdown,
  // e.g. in tests.
  if (!g_browser_process) {
    return;
  }
  // If the BrowserProcess is shutting down, then |profile| may be deleted
  // already. Doing anything here would be dangerous, and |profile| will be
  // deleted very soon in any case.
  if (g_browser_process->IsShuttingDown()) {
    return;
  }
  // |profile_manager| can also be null in tests.
  auto* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager) {
    return;
  }
  // TODO(crbug.com/41484323): |profile| was unexpectedly destroyed
  // early. Convert this to CHECK(profile) once the root cause is fixed.
  if (!profile) {
    DUMP_WILL_BE_NOTREACHED();
    return;
  }
  profile_manager->RemoveKeepAlive(profile.get(), origin);
}
