// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/uninstall_dialog.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/native_window_tracker.h"
#include "chrome/services/app_service/public/cpp/icon_loader.h"
#include "extensions/browser/uninstall_reason.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/apps/app_service/extension_apps.h"
#endif  // OS_CHROMEOS

namespace {

constexpr int32_t kUninstallIconSize = 32;
constexpr int32_t kArcUninstallIconSize = 48;

}  // namespace

namespace apps {

UninstallDialog::UninstallDialog(Profile* profile,
                                 apps::mojom::AppType app_type,
                                 const std::string& app_id,
                                 const std::string& app_name,
                                 apps::mojom::IconKeyPtr icon_key,
                                 apps::IconLoader* icon_loader,
                                 gfx::NativeWindow parent_window,
                                 UninstallCallback uninstall_callback)
    : profile_(profile),
      app_type_(app_type),
      app_id_(app_id),
      app_name_(app_name),
      parent_window_(parent_window),
      uninstall_callback_(std::move(uninstall_callback)) {
  if (parent_window)
    parent_window_tracker_ = NativeWindowTracker::Create(parent_window);

  int32_t size_hint_in_dip;
  switch (app_type) {
    case apps::mojom::AppType::kCrostini:
      // Crostini uninstall dialog doesn't show the icon.
      UiBase::Create(profile_, app_type_, app_id_, app_name, gfx::ImageSkia(),
                     parent_window, this);
      return;
    case apps::mojom::AppType::kArc:
      // Currently ARC apps only support 48*48 native icon.
      size_hint_in_dip = kArcUninstallIconSize;
      break;
    case apps::mojom::AppType::kExtension:
    case apps::mojom::AppType::kWeb:
      UMA_HISTOGRAM_ENUMERATION("Extensions.UninstallSource",
                                extensions::UNINSTALL_SOURCE_APP_LIST,
                                extensions::NUM_UNINSTALL_SOURCES);
      size_hint_in_dip = kUninstallIconSize;
      break;
    default:
      NOTREACHED();
      return;
  }
  constexpr bool kAllowPlaceholderIcon = false;
  icon_loader->LoadIconFromIconKey(
      app_type, app_id, std::move(icon_key),
      apps::mojom::IconCompression::kUncompressed, size_hint_in_dip,
      kAllowPlaceholderIcon,
      base::BindOnce(&UninstallDialog::OnLoadIcon,
                     weak_ptr_factory_.GetWeakPtr()));
}

UninstallDialog::~UninstallDialog() = default;

void UninstallDialog::OnDialogClosed(bool uninstall,
                                     bool clear_site_data,
                                     bool report_abuse) {
#if defined(OS_CHROMEOS)
  if (!uninstall && (app_type_ == apps::mojom::AppType::kExtension ||
                     app_type_ == apps::mojom::AppType::kWeb)) {
    ExtensionApps::RecordUninstallCanceledAction(profile_, app_id_);
  }
#endif  // OS_CHROMEOS

  std::move(uninstall_callback_)
      .Run(uninstall, clear_site_data, report_abuse, this);
}

void UninstallDialog::OnLoadIcon(apps::mojom::IconValuePtr icon_value) {
  if (icon_value->icon_compression !=
      apps::mojom::IconCompression::kUncompressed) {
    OnDialogClosed(false, false, false);
    return;
  }

  if (parent_window_ && parent_window_tracker_->WasNativeWindowClosed()) {
    OnDialogClosed(false, false, false);
    return;
  }

  UiBase::Create(profile_, app_type_, app_id_, app_name_,
                 icon_value->uncompressed, parent_window_, this);
}

}  // namespace apps
