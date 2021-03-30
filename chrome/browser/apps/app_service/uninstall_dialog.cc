// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/uninstall_dialog.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/apps/app_service/app_icon_factory.h"
#include "chrome/browser/apps/app_service/publishers/extension_apps_chromeos.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/native_window_tracker.h"
#include "chrome/common/chrome_features.h"
#include "components/services/app_service/public/cpp/icon_loader.h"
#include "extensions/browser/uninstall_reason.h"

namespace {

constexpr int32_t kUninstallIconSize = 48;

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

  switch (app_type) {
    case apps::mojom::AppType::kArc:
    case apps::mojom::AppType::kBorealis:
    case apps::mojom::AppType::kPluginVm:
      break;
    case apps::mojom::AppType::kCrostini:
      // Crostini icons might be a big image, and not fit the size, so add the
      // resize icon effect, to resize the image.
      icon_key->icon_effects = static_cast<apps::IconEffects>(
          icon_key->icon_effects | apps::IconEffects::kResizeAndPad);
      break;
    case apps::mojom::AppType::kExtension:
    case apps::mojom::AppType::kWeb:
      UMA_HISTOGRAM_ENUMERATION("Extensions.UninstallSource",
                                extensions::UNINSTALL_SOURCE_APP_LIST,
                                extensions::NUM_UNINSTALL_SOURCES);
      break;
    default:
      NOTREACHED();
      return;
  }
  constexpr bool kAllowPlaceholderIcon = false;
  // Currently ARC apps only support 48*48 native icon.
  int32_t size_hint_in_dip = kUninstallIconSize;
  auto icon_type =
      (base::FeatureList::IsEnabled(features::kAppServiceAdaptiveIcon))
          ? apps::mojom::IconType::kStandard
          : apps::mojom::IconType::kUncompressed;
  icon_loader->LoadIconFromIconKey(
      app_type, app_id, std::move(icon_key), icon_type, size_hint_in_dip,
      kAllowPlaceholderIcon,
      base::BindOnce(&UninstallDialog::OnLoadIcon,
                     weak_ptr_factory_.GetWeakPtr()));
}

UninstallDialog::~UninstallDialog() = default;

void UninstallDialog::OnDialogClosed(bool uninstall,
                                     bool clear_site_data,
                                     bool report_abuse) {
  if (!uninstall && (app_type_ == apps::mojom::AppType::kExtension ||
                     app_type_ == apps::mojom::AppType::kWeb)) {
    ExtensionAppsChromeOs::RecordUninstallCanceledAction(profile_, app_id_);
  }

  std::move(uninstall_callback_)
      .Run(uninstall, clear_site_data, report_abuse, this);
}

void UninstallDialog::SetDialogCreatedCallbackForTesting(
    base::OnceClosure callback) {
  dialog_created_callback_ = std::move(callback);
}

void UninstallDialog::OnLoadIcon(apps::mojom::IconValuePtr icon_value) {
  auto icon_type =
      (base::FeatureList::IsEnabled(features::kAppServiceAdaptiveIcon))
          ? apps::mojom::IconType::kStandard
          : apps::mojom::IconType::kUncompressed;
  if (icon_value->icon_type != icon_type) {
    OnDialogClosed(false, false, false);
    return;
  }

  if (parent_window_ && parent_window_tracker_->WasNativeWindowClosed()) {
    OnDialogClosed(false, false, false);
    return;
  }

  UiBase::Create(profile_, app_type_, app_id_, app_name_,
                 icon_value->uncompressed, parent_window_, this);

  // For browser tests, if the callback is set, run the callback to stop the run
  // loop.
  if (!dialog_created_callback_.is_null()) {
    std::move(dialog_created_callback_).Run();
  }
}

}  // namespace apps
