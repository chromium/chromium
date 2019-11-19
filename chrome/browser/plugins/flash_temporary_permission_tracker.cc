// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/flash_temporary_permission_tracker.h"

#include <utility>

#include "base/stl_util.h"
#include "chrome/browser/plugins/flash_temporary_permission_tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/page_transition_types.h"

class FlashTemporaryPermissionTracker::GrantObserver
    : content::WebContentsObserver {
 public:
  GrantObserver(content::WebContents* web_contents,
                const GURL& origin,
                FlashTemporaryPermissionTracker* owner);

  const GURL& origin() { return origin_; }

 private:
  // content::WebContentsObserver
  void WebContentsDestroyed() override;

  GURL origin_;
  FlashTemporaryPermissionTracker* owner_;

  DISALLOW_COPY_AND_ASSIGN(GrantObserver);
};

// static
scoped_refptr<FlashTemporaryPermissionTracker>
FlashTemporaryPermissionTracker::Get(Profile* profile) {
  return FlashTemporaryPermissionTrackerFactory::GetForProfile(profile);
}

FlashTemporaryPermissionTracker::FlashTemporaryPermissionTracker(
    Profile* profile)
    : profile_(profile) {}

FlashTemporaryPermissionTracker::~FlashTemporaryPermissionTracker() {}

bool FlashTemporaryPermissionTracker::IsFlashEnabled(const GURL& url) {
  base::AutoLock lock(granted_origins_lock_);
  return base::Contains(granted_origins_, url.GetOrigin());
}

void FlashTemporaryPermissionTracker::FlashEnabledForWebContents(
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  GURL origin = web_contents->GetLastCommittedURL().GetOrigin();
  {
    base::AutoLock lock(granted_origins_lock_);
    granted_origins_.insert(std::make_pair(
        origin, std::make_unique<GrantObserver>(web_contents, origin, this)));
  }
  content::PluginService::GetInstance()->PurgePluginListCache(profile_, false);
}

void FlashTemporaryPermissionTracker::RevokeAccess(GrantObserver* observer) {
  DCHECK(observer);
  {
    base::AutoLock lock(granted_origins_lock_);
    auto range = granted_origins_.equal_range(observer->origin());
    for (auto it = range.first; it != range.second;) {
      if (it->second.get() == observer) {
        it = granted_origins_.erase(it);
        break;
      }
    }
  }
  content::PluginService::GetInstance()->PurgePluginListCache(profile_, false);
}

void FlashTemporaryPermissionTracker::ShutdownOnUIThread() {
  DCHECK(granted_origins_.empty());
}

FlashTemporaryPermissionTracker::GrantObserver::GrantObserver(
    content::WebContents* web_contents,
    const GURL& origin,
    FlashTemporaryPermissionTracker* owner)
    : content::WebContentsObserver(web_contents),
      origin_(origin),
      owner_(owner) {}

void FlashTemporaryPermissionTracker::GrantObserver::WebContentsDestroyed() {
  owner_->RevokeAccess(this);
}
