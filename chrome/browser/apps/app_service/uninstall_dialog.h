// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_UNINSTALL_DIALOG_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_UNINSTALL_DIALOG_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/widget/widget.h"

class Profile;

namespace gfx {
class ImageSkia;
}

namespace views {
class NativeWindowTracker;
}

namespace apps {
class IconLoader;
class UninstallDialog;
}  // namespace apps

namespace apps {

using OnUninstallForTestingCallback = base::OnceCallback<void(bool)>;
using OnDialogCreatedCallback = base::OnceCallback<void(views::Widget*)>;

// Currently, app uninstallation on Chrome OS invokes a specific dialog per app
// type, Chrome Apps / PWAs, ARC apps and Crostini. There are 3 separate views
// for app uninstalling, which are subtly different from each other.
//
// This UninstallDialog combines the above three specific dialogs, and based on
// different app type to generate different view. Once the user has confirmed,
// the App Service calls the publisher to uninstall the app directly.
//
// TODO(crbug.com/40100977):
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
    explicit UiBase(UninstallDialog* uninstall_dialog)
        : uninstall_dialog_(uninstall_dialog) {}

    UiBase(const UiBase&) = delete;
    UiBase& operator=(const UiBase&) = delete;
    virtual ~UiBase() = default;

    static views::Widget* Create(Profile* profile,
                                 apps::AppType app_type,
                                 const std::string& app_id,
                                 const std::string& app_name,
                                 gfx::ImageSkia image,
                                 gfx::NativeWindow parent_window,
                                 UninstallDialog* uninstall_dialog);

    UninstallDialog* uninstall_dialog() const { return uninstall_dialog_; }

   private:
    raw_ptr<UninstallDialog, AcrossTasksDanglingUntriaged> uninstall_dialog_;
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
                  apps::AppType app_type,
                  const std::string& app_id,
                  const std::string& app_name,
                  gfx::NativeWindow parent_window,
                  UninstallCallback uninstall_callback);
  UninstallDialog(const UninstallDialog&) = delete;
  UninstallDialog& operator=(const UninstallDialog&) = delete;
  ~UninstallDialog();

  // Loads the app icon to show the icon in the uninstall dialog before creating
  // the dialog view.
  void PrepareToShow(IconKey icon_key,
                     apps::IconLoader* icon_loader,
                     int32_t icon_size);

  // Closes this dialog if it is open. If the dialog is not open yet because
  // icons are still loading, immediately runs `uninstall_callback_` so that
  // `this` can be deleted.
  void CloseDialog();

  views::Widget* GetWidget();

  // Called when the uninstall dialog is closing to process uninstall or cancel
  // the uninstall.
  void OnDialogClosed(bool uninstall, bool clear_site_data, bool report_abuse);

  void SetDialogCreatedCallbackForTesting(
      OnUninstallForTestingCallback callback);

 private:
  // Callback invoked when the icon is loaded.
  void OnLoadIcon(IconValuePtr icon_value);

  const raw_ptr<Profile> profile_;
  const apps::AppType app_type_;
  const std::string app_id_;
  const std::string app_name_;
  gfx::NativeWindow parent_window_;
  UninstallCallback uninstall_callback_;

  OnUninstallForTestingCallback uninstall_dialog_created_callback_;

  raw_ptr<views::Widget, AcrossTasksDanglingUntriaged> widget_ = nullptr;

  // Tracks whether |parent_window_| got destroyed.
  std::unique_ptr<views::NativeWindowTracker> parent_window_tracker_;

  base::WeakPtrFactory<UninstallDialog> weak_ptr_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_UNINSTALL_DIALOG_H_
