// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TEST_TEST_DESKS_TEMPLATES_DELEGATE_H_
#define ASH_PUBLIC_CPP_TEST_TEST_DESKS_TEMPLATES_DELEGATE_H_

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/desks_templates_delegate.h"

namespace app_restore {
struct AppLaunchInfo;
}

namespace aura {
class Window;
}

namespace desks_storage {
class DeskModel;
}

namespace ui {
class ColorProvider;
}

namespace ash {

class DeskTemplate;

class ASH_PUBLIC_EXPORT TestDesksTemplatesDelegate
    : public DesksTemplatesDelegate {
 public:
  TestDesksTemplatesDelegate();
  TestDesksTemplatesDelegate(TestDesksTemplatesDelegate&) = delete;
  TestDesksTemplatesDelegate& operator=(TestDesksTemplatesDelegate&) = delete;
  ~TestDesksTemplatesDelegate() override;

  void set_desk_model(desks_storage::DeskModel* desk_model) {
    desk_model_ = desk_model;
  }

  // DesksTemplatesDelegate:
  std::unique_ptr<app_restore::AppLaunchInfo> GetAppLaunchDataForDeskTemplate(
      aura::Window* window) const override;
  desks_storage::DeskModel* GetDeskModel() override;
  bool IsIncognitoWindow(aura::Window* window) const override;
  absl::optional<gfx::ImageSkia> MaybeRetrieveIconForSpecialIdentifier(
      const std::string& identifier,
      const ui::ColorProvider* color_provider) const override;
  void GetFaviconForUrl(const std::string& page_url,
                        int desired_icon_size,
                        favicon_base::FaviconRawBitmapCallback callback,
                        base::CancelableTaskTracker* tracker) const override;
  void GetIconForAppId(const std::string& app_id,
                       int desired_icon_size,
                       base::OnceCallback<void(apps::IconValuePtr icon_value)>
                           callback) const override;
  void LaunchAppsFromTemplate(
      std::unique_ptr<DeskTemplate> desk_template) override;
  bool IsWindowSupportedForDeskTemplate(aura::Window* window) const override;

 private:
  desks_storage::DeskModel* desk_model_ = nullptr;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TEST_TEST_DESKS_TEMPLATES_DELEGATE_H_
