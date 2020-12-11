// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_FLASH_TEMPORARY_PERMISSION_TRACKER_H_
#define CHROME_BROWSER_PLUGINS_FLASH_TEMPORARY_PERMISSION_TRACKER_H_

#include <map>

#include "base/synchronization/lock.h"
#include "components/keyed_service/core/refcounted_keyed_service.h"
#include "url/gurl.h"

class Profile;

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

// This class tracks temporary permission grants to allow Flash to run on web
// pages. This is used when the enterprise plugins setting is set to ASK.
// Temporary permission grants last until all granting WebContents for that
// origin are destroyed. |IsFlashEnabled| can be called from any thread.
class FlashTemporaryPermissionTracker : public RefcountedKeyedService {
 public:
  static scoped_refptr<FlashTemporaryPermissionTracker> Get(
      content::BrowserContext* browser_context);

  // Returns true if Flash is enabled for a given |url|. Can be called from any
  // thread.
  bool IsFlashEnabled(const GURL& url);

  // Call if Flash was enabled in the main frame of a given |web_contents|.
  // Flash is then enabled for the origin of |web_contents| profile-wide until
  // all granting |web_contents| of that origin are destroyed.
  void FlashEnabledForWebContents(content::WebContents* web_contents);

 private:
  friend class FlashTemporaryPermissionTrackerFactory;
  friend class FlashTemporaryPermissionTrackerTest;

  class GrantObserver;

  explicit FlashTemporaryPermissionTracker(Profile* profile);
  ~FlashTemporaryPermissionTracker() override;

  // Revoke access from the given |origin| for a given |web_contents|.
  void RevokeAccess(GrantObserver* observer);

  // RefCountedProfileKeyedBase method override.
  void ShutdownOnUIThread() override;

  Profile* profile_;

  // We use GURLs to store the origins because we need to support the file:
  // scheme comparing equal to itself.
  // TODO(raymes): Revisit this after we decide what to do with plugins on file:
  // URLs.
  std::multimap<GURL, std::unique_ptr<GrantObserver>> granted_origins_;

  // Lock to protect |granted_origins_|. This is needed because IsFlashEnabled
  // may be called from any thread via the ChromePluginServiceFilter.
  base::Lock granted_origins_lock_;

  DISALLOW_COPY_AND_ASSIGN(FlashTemporaryPermissionTracker);
};

#endif  // CHROME_BROWSER_PLUGINS_FLASH_TEMPORARY_PERMISSION_TRACKER_H_
