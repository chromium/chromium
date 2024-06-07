// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_UNINSTALL_DIALOG_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_UNINSTALL_DIALOG_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/extensions/chrome_app_icon.h"
#include "chrome/browser/extensions/chrome_app_icon_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/uninstall_reason.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/native_widget_types.h"
#include "url/gurl.h"

namespace views {
class NativeWindowTracker;
}

namespace extensions {
class Extension;

class ExtensionUninstallDialog : public ChromeAppIconDelegate,
                                 public ExtensionRegistryObserver,
                                 public ProfileObserver {
 public:
  // The type of action the dialog took at close.
  // Do not reorder this enum as it is used in UMA histograms.
  enum CloseAction {
    CLOSE_ACTION_UNINSTALL = 0,
    CLOSE_ACTION_UNINSTALL_AND_CHECKBOX_CHECKED = 1,
    CLOSE_ACTION_CANCELED = 2,
    CLOSE_ACTION_LAST = 3,
  };

  // TODO(devlin): For a single method like this, a callback is probably more
  // appropriate than a delegate.
  class Delegate {
   public:
    // Called when the dialog closes.
    // |did_start_uninstall| indicates whether the uninstall process for the
    // extension started. If this is false, |error| will contain the reason.
    virtual void OnExtensionUninstallDialogClosed(bool did_start_uninstall,
                                                  const std::u16string& error) {
    }

   protected:
    virtual ~Delegate() {}
  };

  // Creates the Views implementation of ExtensionUninstallDialog. The dialog
  // will be modal to `parent`, or a non-modal dialog if `parent` is NULL.
  static std::unique_ptr<ExtensionUninstallDialog>
  Create(Profile* profile, gfx::NativeWindow parent, Delegate* delegate);

  ExtensionUninstallDialog(const ExtensionUninstallDialog&) = delete;
  ExtensionUninstallDialog& operator=(const ExtensionUninstallDialog&) = delete;

  ~ExtensionUninstallDialog() override;

  // This is called to verify whether the uninstallation should proceed.
  // Starts the process of showing a confirmation UI, which is split into two.
  // 1) Set off a 'load icon' task.
  // 2) Handle the load icon response and show the UI (OnImageLoaded).
  void ConfirmUninstall(const scoped_refptr<const Extension>& extension,
                        UninstallReason reason,
                        UninstallSource source);

  // This shows the same dialog as above, except it also shows which extension
  // triggered the dialog.
  void ConfirmUninstallByExtension(
      const scoped_refptr<const Extension>& extension,
      const scoped_refptr<const Extension>& triggering_extension,
      UninstallReason reason,
      UninstallSource source);

  // Returns true if a checkbox should be shown in the dialog.
  bool ShouldShowCheckbox() const;

  // Called when the dialog is closing to do any book-keeping.
  void OnDialogClosed(CloseAction action);

  base::WeakPtr<ExtensionUninstallDialog> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // Called from unit test to check callbacks in dialog.
  static void SetOnShownCallbackForTesting(base::RepeatingClosure* callback);

 protected:
  // Constructor used by the derived classes.
  ExtensionUninstallDialog(Profile* profile,
                           gfx::NativeWindow parent,
                           Delegate* delegate);

  // Accessors for members.
  const Extension* extension() const { return extension_.get(); }
  const Extension* triggering_extension() const {
      return triggering_extension_.get(); }
  const gfx::ImageSkia& icon() const { return icon_->image_skia(); }
  gfx::NativeWindow parent() { return parent_; }

 private:
  // Uninstalls the extension. Returns true on success, and populates |error| on
  // failure.
  bool Uninstall(std::u16string* error);

  // Handles the "report abuse" checkbox being checked at the close of the
  // dialog.
  void HandleReportAbuse();

  // ChromeAppIconDelegate:
  void OnIconUpdated(ChromeAppIcon* icon) override;

  // ExtensionRegistryObserver:
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const Extension* extension,
                              UninstallReason reason) override;

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

  // Displays the prompt. This should only be called after loading the icon.
  // The implementations of this method are platform-specific.
  virtual void Show() = 0;

  // Forcefully closes the dialog view.
  virtual void Close() = 0;

  // Resets to nullptr when the Profile is deleted.
  raw_ptr<Profile> profile_;

  // The dialog's parent window.
  gfx::NativeWindow parent_;

  // The delegate we will call Accepted/Canceled on after confirmation dialog.
  raw_ptr<Delegate> delegate_;

  // The extension we are showing the dialog for.
  scoped_refptr<const Extension> extension_;

  // The extension triggering the dialog if the dialog was shown by
  // chrome.management.uninstall.
  scoped_refptr<const Extension> triggering_extension_;

  std::unique_ptr<ChromeAppIcon> icon_;

  // Tracks whether |parent_| got destroyed.
  std::unique_ptr<views::NativeWindowTracker> parent_window_tracker_;

  // Indicates that dialog was shown.
  bool dialog_shown_ = false;

  // True if a checkbox for reporting abuse is shown.
  bool show_report_abuse_checkbox_ = false;

  // Whether the extension was uninstalled before the user closed the dialog
  // (e.g. by another source).
  bool extension_uninstalled_early_ = false;

  UninstallReason uninstall_reason_ = UNINSTALL_REASON_FOR_TESTING;

  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      registry_observation_{this};
  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};

  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<ExtensionUninstallDialog> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_UNINSTALL_DIALOG_H_
