// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_SYSTEM_SYSTEM_MEDIA_PERMISSION_CACHE_H_
#define CHROME_BROWSER_PERMISSIONS_SYSTEM_SYSTEM_MEDIA_PERMISSION_CACHE_H_

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "chrome/browser/permissions/system/system_permission_common.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "components/content_settings/core/common/content_settings_types.h"

namespace system_permission_settings {

// Reusable platform-agnostic media permission status cache.
// Encapsulates window activation listening, off-main-thread OS permission
// checks, and state caching for camera and microphone system permissions.
class SystemMediaPermissionCache : public BrowserCollectionObserver {
 public:
  using CheckMediaPermissionCallback =
      base::RepeatingCallback<SystemPermission()>;
  using CreateTaskRunnerCallback =
      base::OnceCallback<scoped_refptr<base::SequencedTaskRunner>(
          const base::TaskTraits&)>;

  SystemMediaPermissionCache(
      CreateTaskRunnerCallback create_task_runner_cb,
      CheckMediaPermissionCallback check_camera_permission_cb,
      CheckMediaPermissionCallback check_mic_permission_cb);
  SystemMediaPermissionCache(const SystemMediaPermissionCache&) = delete;
  SystemMediaPermissionCache& operator=(const SystemMediaPermissionCache&) =
      delete;
  ~SystemMediaPermissionCache() override;

  // BrowserCollectionObserver:
  void OnBrowserActivated(BrowserWindowInterface* browser) override;

  // Core status accessors:
  bool CanPrompt(ContentSettingsType type) const;
  bool IsDenied(ContentSettingsType type) const;
  bool IsAllowed(ContentSettingsType type) const;

  // Performs a fresh OS check for `type` on a thread pool worker thread,
  // updates the cached entry, and invokes `callback` with whether it is denied.
  void IsDeniedFresh(ContentSettingsType type,
                     SystemPermissionDeniedCallback callback);

  // Manually refreshes cached camera and microphone permissions asynchronously.
  void RefreshSystemPermissionSettings(
      base::OnceClosure callback = base::DoNothing());

 private:
  struct SystemPermissionState {
    SystemPermission camera = SystemPermission::kNotDetermined;
    SystemPermission mic = SystemPermission::kNotDetermined;
  };

  SystemPermission GetCachedPermission(ContentSettingsType type) const;
  void UpdateCacheEntry(ContentSettingsType type, SystemPermission result);

  void OnPermissionsRefreshed(SystemPermissionState state);

  // This is static to ensure that the reply callback (and its `.Then()`
  // continuation) always runs even if the `SystemMediaPermissionCache` instance
  // is destroyed. If it was bound as an instance method with a WeakPtr, the
  // callback chain would be cancelled, dropping the caller's callback.
  static bool ProcessDeniedPermissionResult(
      base::WeakPtr<SystemMediaPermissionCache> cache,
      ContentSettingsType type,
      SystemPermission result);

  base::SequencedTaskRunner* GetTaskRunner();

  CreateTaskRunnerCallback create_task_runner_cb_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  CheckMediaPermissionCallback check_camera_permission_cb_;
  CheckMediaPermissionCallback check_mic_permission_cb_;

  SystemPermission camera_status_ = SystemPermission::kNotDetermined;
  SystemPermission mic_status_ = SystemPermission::kNotDetermined;

  base::ScopedObservation<GlobalBrowserCollection, BrowserCollectionObserver>
      browser_collection_observation_{this};
  base::WeakPtrFactory<SystemMediaPermissionCache> weak_factory_{this};
};

}  // namespace system_permission_settings

#endif  // CHROME_BROWSER_PERMISSIONS_SYSTEM_SYSTEM_MEDIA_PERMISSION_CACHE_H_
