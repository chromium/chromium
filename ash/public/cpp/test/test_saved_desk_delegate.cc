// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/test/test_saved_desk_delegate.h"

#include "ash/public/cpp/desk_template.h"
#include "base/containers/contains.h"
#include "components/app_restore/app_launch_info.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

TestSavedDeskDelegate::TestSavedDeskDelegate() = default;

TestSavedDeskDelegate::~TestSavedDeskDelegate() = default;

void TestSavedDeskDelegate::GetAppLaunchDataForSavedDesk(
    aura::Window* window,
    GetAppLaunchDataCallback callback) const {
  std::move(callback).Run({});
}

desks_storage::DeskModel* TestSavedDeskDelegate::GetDeskModel() {
  return desk_model_;
}

desks_storage::AdminTemplateService*
TestSavedDeskDelegate::GetAdminTemplateService() {
  return admin_template_service_;
}

bool TestSavedDeskDelegate::IsWindowPersistable(aura::Window* window) const {
  return true;
}

std::optional<gfx::ImageSkia>
TestSavedDeskDelegate::MaybeRetrieveIconForSpecialIdentifier(
    const std::string& identifier,
    const ui::ColorProvider* color_provider) const {
  return std::nullopt;
}

void TestSavedDeskDelegate::GetFaviconForUrl(
    const std::string& page_url,
    uint64_t lacros_profile_id,
    base::OnceCallback<void(const gfx::ImageSkia&)> callback,
    base::CancelableTaskTracker* tracker) const {
  std::move(callback).Run(gfx::ImageSkia());
}

void TestSavedDeskDelegate::GetIconForAppId(
    const std::string& app_id,
    int desired_icon_size,
    base::OnceCallback<void(const gfx::ImageSkia&)> callback) const {
  // `default_app_icon_` will be null if not set.
  if (!default_app_icon_.isNull()) {
    std::move(callback).Run(default_app_icon_);
  }
}

void TestSavedDeskDelegate::LaunchAppsFromSavedDesk(
    std::unique_ptr<DeskTemplate> saved_desk) {}

bool TestSavedDeskDelegate::IsWindowSupportedForSavedDesk(
    aura::Window* window) const {
  return DeskTemplate::IsAppTypeSupported(window);
}

std::string TestSavedDeskDelegate::GetAppShortName(const std::string& app_id) {
  return std::string();
}

bool TestSavedDeskDelegate::IsAppAvailable(const std::string& app_id) const {
  return !base::Contains(unavailable_app_ids_, app_id);
}

}  // namespace ash
