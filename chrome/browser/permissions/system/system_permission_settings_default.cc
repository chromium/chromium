// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "build/buildflag.h"
#include "chrome/browser/permissions/system/platform_handle.h"
#include "chrome/browser/permissions/system/system_permission_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"

static_assert(!BUILDFLAG(IS_CHROMEOS_ASH));
static_assert(!BUILDFLAG(IS_MAC));

namespace system_permission_settings {

namespace {

class PlatformHandleImpl : public PlatformHandle {
  bool CanPrompt(ContentSettingsType type) override { return false; }

  bool IsDenied(ContentSettingsType type) override { return false; }

  bool IsAllowed(ContentSettingsType type) override { return true; }

  void OpenSystemSettings(content::WebContents*,
                          ContentSettingsType type) override {
    // no-op
    NOTREACHED();
  }

  void Request(ContentSettingsType type,
               SystemPermissionResponseCallback callback) override {
    std::move(callback).Run();
    NOTREACHED();
  }

  std::unique_ptr<ScopedObservation> Observe(
      SystemPermissionChangedCallback observer) override {
    return nullptr;
  }
};

}  // namespace

std::unique_ptr<PlatformHandle> PlatformHandle::Create() {
  return std::make_unique<PlatformHandleImpl>();
}

}  // namespace system_permission_settings
