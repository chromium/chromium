// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_UNINSTALL_DIALOG_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_UNINSTALL_DIALOG_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"
#include "ui/gfx/native_widget_types.h"

class NativeWindowTracker;
class Profile;

namespace gfx {
class ImageSkia;
}

namespace apps {
class IconLoader;
class UninstallDialog;
}  // namespace apps

namespace apps {

// Currently, app uninstallation on Chrome OS invokes a specific dialog per app
// type, Chrome Apps / PWAs, ARC apps and Crostini. There are 3 separate views
// for app uninstalling, which are subtly different from each other.
//
// This UninstallDialog combines the above three specific dialogs, and based on
// different app type to generate different view. Once the user has confirmed,
// the App Service calls the publisher to uninstall the app directly.
//
// TODO(crbug.com/1009248):
// 1. Add an interface to the uninstall, like what is done by
// extension_uninstall_dialog_->ConfirmUninstallByExtension
class UninstallDialog {
 public:
  // The UiBase is the parent virtual class for the AppUninstallDialogView,
  // which is located in
  // chrome/browser/ui/view/apps/app_uninstall_dialog_view.h. The UiBase is also
  // used to connect the UninstallDialog and AppUninstallDialogView, to transfer
  // the icon image, and the callback function.
  class UiBase {
   public:
    explicit UiBase(gfx::ImageSkia image, UninstallDialog* uninstall_dialog)
        : image_(image), uninstall_dialog_(uninstall_dialog) {}

    virtual ~UiBase() = default;

    static void Create(Profile* profile,
                       apps::mojom::AppType app_type,
                       const std::string& app_id,
                       const std::string& app_name,
                       gfx::ImageSkia image,
                       gfx::NativeWindow parent_window,
                       UninstallDialog* uninstall_dialog);

    gfx::ImageSkia image() const { return image_; }
    UninstallDialog* uninstall_dialog() const { return uninstall_dialog_; }

   private:
    gfx::ImageSkia image_;
    UninstallDialog* uninstall_dialog_;

    DISALLOW_COPY_AND_ASSIGN(UiBase);
  };

  // Called when the dialog closes after the user has made a decision about
  // whether to uninstall the app. If |clear_site_data| is true, site data will
  // be removed after uninstalling the app. Only ever true for PWAs. If
  // |report_abuse| is true, report abuse after uninstalling the app. Only ever
  // true for Chrome Apps.
  using UninstallCallback =
      base::OnceCallback<void(bool uninstall,
                              bool clear_site_data,
                              bool report_rebuse,
                              UninstallDialog* uninstall_dialog)>;

  UninstallDialog(Profile* profile,
                  apps::mojom::AppType app_type,
                  const std::string& app_id,
                  const std::string& app_name,
                  apps::mojom::IconKeyPtr icon_key,
                  IconLoader* icon_loader,
                  gfx::NativeWindow parent_window,
                  UninstallCallback uninstall_callback);
  ~UninstallDialog();

  // Called when the uninstall dialog is closing to process uninstall or cancel
  // the uninstall.
  void OnDialogClosed(bool uninstall, bool clear_site_data, bool report_abuse);

 private:
  // Callback invoked when the icon is loaded.
  void OnLoadIcon(apps::mojom::IconValuePtr icon_value);

  Profile* profile_;
  apps::mojom::AppType app_type_;
  const std::string app_id_;
  const std::string app_name_;
  gfx::NativeWindow parent_window_;
  UninstallCallback uninstall_callback_;

  // Tracks whether |parent_window_| got destroyed.
  std::unique_ptr<NativeWindowTracker> parent_window_tracker_;

  base::WeakPtrFactory<UninstallDialog> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(UninstallDialog);
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_UNINSTALL_DIALOG_H_
