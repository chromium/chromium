// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/system/system_media_permission_cache.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/after_startup_task_utils.h"
#include "content/public/browser/browser_thread.h"

namespace system_permission_settings {

SystemMediaPermissionCache::SystemMediaPermissionCache(
    CreateTaskRunnerCallback create_task_runner_cb,
    CheckMediaPermissionCallback check_camera_permission_cb,
    CheckMediaPermissionCallback check_mic_permission_cb)
    : create_task_runner_cb_(std::move(create_task_runner_cb)),
      check_camera_permission_cb_(std::move(check_camera_permission_cb)),
      check_mic_permission_cb_(std::move(check_mic_permission_cb)) {
  CHECK(check_camera_permission_cb_);
  CHECK(check_mic_permission_cb_);
  if (content::BrowserThread::IsThreadInitialized(content::BrowserThread::UI)) {
    AfterStartupTaskUtils::PostTask(
        FROM_HERE, base::SequencedTaskRunner::GetCurrentDefault(),
        base::BindOnce(
            &SystemMediaPermissionCache::RefreshSystemPermissionSettings,
            weak_factory_.GetWeakPtr(), base::DoNothing()));
  } else if (base::SequencedTaskRunner::HasCurrentDefault()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &SystemMediaPermissionCache::RefreshSystemPermissionSettings,
            weak_factory_.GetWeakPtr(), base::DoNothing()));
  } else if (auto* global_browser_collection =
                 GlobalBrowserCollection::GetInstance()) {
    browser_collection_observation_.Observe(global_browser_collection);
  }
}

SystemMediaPermissionCache::~SystemMediaPermissionCache() = default;

void SystemMediaPermissionCache::OnBrowserActivated(
    BrowserWindowInterface* browser) {
  RefreshSystemPermissionSettings();
}

bool SystemMediaPermissionCache::CanPrompt(ContentSettingsType type) const {
  return IsSystemPermissionPrompt(GetCachedPermission(type));
}

bool SystemMediaPermissionCache::IsDenied(ContentSettingsType type) const {
  return IsSystemPermissionDenied(GetCachedPermission(type));
}

bool SystemMediaPermissionCache::IsAllowed(ContentSettingsType type) const {
  return IsSystemPermissionAllowed(GetCachedPermission(type));
}

void SystemMediaPermissionCache::IsDeniedFresh(
    ContentSettingsType type,
    SystemPermissionDeniedCallback callback) {
  if (type == ContentSettingsType::MEDIASTREAM_MIC) {
    GetTaskRunner()->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce([](CheckMediaPermissionCallback cb) { return cb.Run(); },
                       check_mic_permission_cb_),
        base::BindOnce(
            &SystemMediaPermissionCache::ProcessDeniedPermissionResult,
            weak_factory_.GetWeakPtr(), type)
            .Then(std::move(callback)));
    return;
  }
  if (type == ContentSettingsType::MEDIASTREAM_CAMERA ||
      type == ContentSettingsType::CAMERA_PAN_TILT_ZOOM) {
    GetTaskRunner()->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce([](CheckMediaPermissionCallback cb) { return cb.Run(); },
                       check_camera_permission_cb_),
        base::BindOnce(
            &SystemMediaPermissionCache::ProcessDeniedPermissionResult,
            weak_factory_.GetWeakPtr(), type)
            .Then(std::move(callback)));
    return;
  }
  std::move(callback).Run(IsDenied(type));
}

void SystemMediaPermissionCache::RefreshSystemPermissionSettings(
    base::OnceClosure callback) {
  GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](CheckMediaPermissionCallback camera_cb,
             CheckMediaPermissionCallback mic_cb) {
            return SystemPermissionState{camera_cb.Run(), mic_cb.Run()};
          },
          check_camera_permission_cb_, check_mic_permission_cb_),
      base::BindOnce(&SystemMediaPermissionCache::OnPermissionsRefreshed,
                     weak_factory_.GetWeakPtr())
          .Then(std::move(callback)));
}

base::SequencedTaskRunner* SystemMediaPermissionCache::GetTaskRunner() {
  if (!task_runner_) {
    task_runner_ =
        std::move(create_task_runner_cb_)
            .Run({base::MayBlock(), base::TaskPriority::BEST_EFFORT});
  }
  return task_runner_.get();
}

SystemPermission SystemMediaPermissionCache::GetCachedPermission(
    ContentSettingsType type) const {
  if (type == ContentSettingsType::MEDIASTREAM_CAMERA ||
      type == ContentSettingsType::CAMERA_PAN_TILT_ZOOM) {
    return camera_status_;
  }
  if (type == ContentSettingsType::MEDIASTREAM_MIC) {
    return mic_status_;
  }
  NOTREACHED();
}

void SystemMediaPermissionCache::UpdateCacheEntry(ContentSettingsType type,
                                                  SystemPermission result) {
  if (type == ContentSettingsType::MEDIASTREAM_CAMERA ||
      type == ContentSettingsType::CAMERA_PAN_TILT_ZOOM) {
    camera_status_ = result;
  } else if (type == ContentSettingsType::MEDIASTREAM_MIC) {
    mic_status_ = result;
  }
}

void SystemMediaPermissionCache::OnPermissionsRefreshed(
    SystemPermissionState state) {
  UpdateCacheEntry(ContentSettingsType::MEDIASTREAM_CAMERA, state.camera);
  UpdateCacheEntry(ContentSettingsType::MEDIASTREAM_MIC, state.mic);
  if (!browser_collection_observation_.IsObserving()) {
    if (auto* global_browser_collection =
            GlobalBrowserCollection::GetInstance()) {
      browser_collection_observation_.Observe(global_browser_collection);
    }
  }
}

// static
bool SystemMediaPermissionCache::ProcessDeniedPermissionResult(
    base::WeakPtr<SystemMediaPermissionCache> cache,
    ContentSettingsType type,
    SystemPermission result) {
  if (cache) {
    cache->UpdateCacheEntry(type, result);
  }
  return IsSystemPermissionDenied(result);
}

}  // namespace system_permission_settings
