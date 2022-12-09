// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/uninstall_dialog.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/publishers/extension_apps_chromeos.h"
#include "chrome/browser/profiles/profile.h"
#include "components/services/app_service/public/cpp/icon_loader.h"
#include "extensions/browser/uninstall_reason.h"
#include "ui/views/native_window_tracker.h"

namespace {

constexpr int32_t kUninstallIconSize = 48;

}  // namespace

namespace apps {

UninstallDialog::UninstallDialog(Profile* profile,
                                 apps::AppType app_type,
                                 const std::string& app_id,
                                 const std::string& app_name,
                                 gfx::NativeWindow parent_window,
                                 UninstallCallback uninstall_callback)
    : profile_(profile),
      app_type_(app_type),
      app_id_(app_id),
      app_name_(app_name),
      parent_window_(parent_window),
      uninstall_callback_(std::move(uninstall_callback)) {
  if (parent_window)
    parent_window_tracker_ = views::NativeWindowTracker::Create(parent_window);
}

UninstallDialog::~UninstallDialog() = default;

void UninstallDialog::PrepareToShow(IconKey icon_key,
                                    apps::IconLoader* icon_loader) {
  switch (app_type_) {
    case apps::AppType::kArc:
    case apps::AppType::kBorealis:
    case apps::AppType::kPluginVm:
      break;
    case apps::AppType::kCrostini:
      // Crostini icons might be a big image, and not fit the size, so add the
      // resize icon effect, to resize the image.
      icon_key.icon_effects = static_cast<apps::IconEffects>(
          icon_key.icon_effects | apps::IconEffects::kMdIconStyle);
      break;
    case apps::AppType::kChromeApp:
    case apps::AppType::kStandaloneBrowserChromeApp:
    case apps::AppType::kWeb:
      UMA_HISTOGRAM_ENUMERATION("Extensions.UninstallSource",
                                extensions::UNINSTALL_SOURCE_APP_LIST,
                                extensions::NUM_UNINSTALL_SOURCES);
      break;
    default:
      NOTREACHED();
      return;
  }

  // Currently ARC apps only support 48*48 native icon.
  icon_loader->LoadIconFromIconKey(
      app_type_, app_id_, icon_key, IconType::kStandard, kUninstallIconSize,
      /*allow_placeholder_icon=*/false,
      base::BindOnce(&UninstallDialog::OnLoadIcon,
                     weak_ptr_factory_.GetWeakPtr()));
}

views::Widget* UninstallDialog::GetWidget() {
  return widget_;
}

void UninstallDialog::OnDialogClosed(bool uninstall,
                                     bool clear_site_data,
                                     bool report_abuse) {
  std::move(uninstall_callback_)
      .Run(uninstall, clear_site_data, report_abuse, this);
}

void UninstallDialog::SetDialogCreatedCallbackForTesting(
    OnUninstallForTestingCallback callback) {
  uninstall_dialog_created_callback_ = std::move(callback);
}

void UninstallDialog::OnLoadIcon(IconValuePtr icon_value) {
  auto icon_type = IconType::kStandard;
  if (icon_value->icon_type != icon_type) {
    OnDialogClosed(false, false, false);
    return;
  }

  if (parent_window_ && parent_window_tracker_->WasNativeWindowDestroyed()) {
    OnDialogClosed(false, false, false);
    return;
  }

  widget_ = UiBase::Create(profile_, app_type_, app_id_, app_name_,
                           icon_value->uncompressed, parent_window_, this);

  // For browser tests, if the callback is set, run the callback to stop the run
  // loop.
  if (!uninstall_dialog_created_callback_.is_null()) {
    std::move(uninstall_dialog_created_callback_).Run(true);
  }
}

}  // namespace apps
