// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/system/system_permission_settings.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/privacy_hub/privacy_hub_util.h"
#include "chrome/browser/permissions/system/platform_handle.h"
#include "chrome/browser/web_applications/manifest_update_utils.h"
#include "components/content_settings/core/common/content_settings_types.h"

namespace system_permission_settings {

namespace {

class PlatformObservationWrapper : public ScopedObservation {
 public:
  explicit PlatformObservationWrapper(
      std::unique_ptr<ash::privacy_hub_util::ContentBlockObservation>
          observation)
      : ScopedObservation(), observation_(std::move(observation)) {}

 private:
  std::unique_ptr<ash::privacy_hub_util::ContentBlockObservation> observation_;
};

class PlatformHandleImpl : public PlatformHandle {
 public:
  bool CanPrompt(ContentSettingsType type) override { return false; }

  bool IsDenied(ContentSettingsType type) override {
      return ash::privacy_hub_util::ContentBlocked(type);
  }

  bool IsAllowed(ContentSettingsType type) override { return !IsDenied(type); }

  void OpenSystemSettings(content::WebContents*,
                          ContentSettingsType type) override {
    ash::privacy_hub_util::OpenSystemSettings(type);
  }

  void Request(ContentSettingsType type,
               SystemPermissionResponseCallback callback) override {
    std::move(callback).Run();
    NOTREACHED();
  }

  std::unique_ptr<ScopedObservation> Observe(
      SystemPermissionChangedCallback observer) override {
    return make_unique<PlatformObservationWrapper>(
        ash::privacy_hub_util::CreateObservationForBlockedContent(
            std::move(observer)));
  }
};

}  // namespace

std::unique_ptr<PlatformHandle> PlatformHandle::Create() {
  return std::make_unique<PlatformHandleImpl>();
}

}  // namespace system_permission_settings
