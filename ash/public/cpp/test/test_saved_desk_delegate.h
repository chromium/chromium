// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TEST_TEST_SAVED_DESK_DELEGATE_H_
#define ASH_PUBLIC_CPP_TEST_TEST_SAVED_DESK_DELEGATE_H_

#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/saved_desk_delegate.h"
#include "base/memory/raw_ptr.h"
#include "ui/gfx/image/image_skia.h"

namespace aura {
class Window;
}

namespace desks_storage {
class DeskModel;
class AdminTemplateService;
}

namespace ui {
class ColorProvider;
}

namespace ash {

class DeskTemplate;

class ASH_PUBLIC_EXPORT TestSavedDeskDelegate : public SavedDeskDelegate {
 public:
  TestSavedDeskDelegate();
  TestSavedDeskDelegate(TestSavedDeskDelegate&) = delete;
  TestSavedDeskDelegate& operator=(TestSavedDeskDelegate&) = delete;
  ~TestSavedDeskDelegate() override;

  void set_desk_model(desks_storage::DeskModel* desk_model) {
    desk_model_ = desk_model;
  }

  void set_admin_template_service(
      desks_storage::AdminTemplateService* admin_template_service) {
    admin_template_service_ = admin_template_service;
  }

  void set_unavailable_apps(
      const std::vector<std::string>& unavailable_app_ids) {
    unavailable_app_ids_ = unavailable_app_ids;
  }

  void set_default_app_icon(const gfx::ImageSkia& default_app_icon) {
    default_app_icon_ = default_app_icon;
  }

  // SavedDeskDelegate:
  void GetAppLaunchDataForSavedDesk(
      aura::Window* window,
      GetAppLaunchDataCallback callback) const override;
  desks_storage::DeskModel* GetDeskModel() override;
  desks_storage::AdminTemplateService* GetAdminTemplateService() override;
  bool IsWindowPersistable(aura::Window* window) const override;
  std::optional<gfx::ImageSkia> MaybeRetrieveIconForSpecialIdentifier(
      const std::string& identifier,
      const ui::ColorProvider* color_provider) const override;
  void GetFaviconForUrl(
      const std::string& page_url,
      uint64_t lacros_profile_id,
      base::OnceCallback<void(const gfx::ImageSkia&)> callback,
      base::CancelableTaskTracker* tracker) const override;
  void GetIconForAppId(
      const std::string& app_id,
      int desired_icon_size,
      base::OnceCallback<void(const gfx::ImageSkia&)> callback) const override;
  void LaunchAppsFromSavedDesk(
      std::unique_ptr<DeskTemplate> saved_desk) override;
  bool IsWindowSupportedForSavedDesk(aura::Window* window) const override;
  std::string GetAppShortName(const std::string& app_id) override;
  bool IsAppAvailable(const std::string& app_id) const override;

 private:
  raw_ptr<desks_storage::DeskModel> desk_model_ = nullptr;
  raw_ptr<desks_storage::AdminTemplateService, DanglingUntriaged>
      admin_template_service_ = nullptr;
  std::vector<std::string> unavailable_app_ids_;
  gfx::ImageSkia default_app_icon_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TEST_TEST_SAVED_DESK_DELEGATE_H_
