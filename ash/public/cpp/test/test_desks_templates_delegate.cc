// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/test/test_desks_templates_delegate.h"

#include "ash/public/cpp/desk_template.h"
#include "components/app_restore/app_launch_info.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

TestDesksTemplatesDelegate::TestDesksTemplatesDelegate() = default;

TestDesksTemplatesDelegate::~TestDesksTemplatesDelegate() = default;

std::unique_ptr<app_restore::AppLaunchInfo>
TestDesksTemplatesDelegate::GetAppLaunchDataForDeskTemplate(
    aura::Window* window) const {
  return nullptr;
}

desks_storage::DeskModel* TestDesksTemplatesDelegate::GetDeskModel() {
  return desk_model_;
}

bool TestDesksTemplatesDelegate::IsIncognitoWindow(aura::Window* window) const {
  return false;
}

absl::optional<gfx::ImageSkia>
TestDesksTemplatesDelegate::MaybeRetrieveIconForSpecialIdentifier(
    const std::string& identifier,
    const ui::ColorProvider* color_provider) const {
  return absl::nullopt;
}

void TestDesksTemplatesDelegate::GetFaviconForUrl(
    const std::string& page_url,
    int desired_icon_size,
    favicon_base::FaviconRawBitmapCallback callback,
    base::CancelableTaskTracker* tracker) const {}

void TestDesksTemplatesDelegate::GetIconForAppId(
    const std::string& app_id,
    int desired_icon_size,
    base::OnceCallback<void(apps::IconValuePtr icon_value)> callback) const {}

void TestDesksTemplatesDelegate::LaunchAppsFromTemplate(
    std::unique_ptr<DeskTemplate> desk_template) {}

bool TestDesksTemplatesDelegate::IsWindowSupportedForDeskTemplate(
    aura::Window* window) const {
  return DeskTemplate::IsAppTypeSupported(window);
}

}  // namespace ash
