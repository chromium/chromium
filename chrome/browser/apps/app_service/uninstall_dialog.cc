// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/uninstall_dialog.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/publishers/extension_apps_chromeos.h"
#include "chrome/browser/profiles/profile.h"
#include "components/services/app_service/public/cpp/icon_loader.h"
#include "extensions/browser/uninstall_reason.h"
#include "ui/views/native_window_tracker.h"
#include "ui/views/widget/widget.h"

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
  if (parent_window) {
    parent_window_tracker_ = views::NativeWindowTracker::Create(parent_window);
  }
}

UninstallDialog::~UninstallDialog() = default;

void UninstallDialog::PrepareToShow(IconKey icon_key,
                                    apps::IconLoader* icon_loader,
                                    int32_t icon_size) {
  if (app_type_ == AppType::kCrostini) {
    // Crostini icons might be a big image, and not fit the size, so add the
    // resize icon effect, to resize the image.
    icon_key.icon_effects = static_cast<apps::IconEffects>(
        icon_key.icon_effects | apps::IconEffects::kMdIconStyle);
  }

  if (app_type_ == AppType::kChromeApp ||
      app_type_ == AppType::kStandaloneBrowserChromeApp) {
    UMA_HISTOGRAM_ENUMERATION("Extensions.UninstallSource",
                              extensions::UNINSTALL_SOURCE_APP_LIST,
                              extensions::NUM_UNINSTALL_SOURCES);
  }

  // Currently ARC apps only support 48*48 native icon.
  icon_loader->LoadIconFromIconKey(
      app_id_, icon_key, IconType::kStandard, icon_size,
      /*allow_placeholder_icon=*/false,
      base::BindOnce(&UninstallDialog::OnLoadIcon,
                     weak_ptr_factory_.GetWeakPtr()));
}

void UninstallDialog::CloseDialog() {
  if (widget_) {
    widget_->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
    return;
  }

  OnDialogClosed(false, false, false);
}

views::Widget* UninstallDialog::GetWidget() {
  return widget_;
}

void UninstallDialog::OnDialogClosed(bool uninstall,
                                     bool clear_site_data,
                                     bool report_abuse) {
  CHECK(uninstall_callback_);
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

  if (uninstall_dialog_created_callback_) {
    std::move(uninstall_dialog_created_callback_).Run(true);
  }
}

}  // namespace apps
