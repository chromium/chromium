// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_PROMPT_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_PROMPT_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/threading/thread_checker.h"
#include "base/values.h"
#include "chrome/browser/extensions/install_prompt_permissions.h"
#include "chrome/browser/ui/extensions/extension_install_ui.h"
#include "chrome/common/buildflags.h"
#include "extensions/common/permissions/permission_message.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/native_widget_types.h"

class ExtensionInstallPromptShowParams;
class Profile;

namespace content {
class BrowserContext;
class WebContents;
}

namespace extensions {
class CrxInstallError;
class Extension;
class PermissionSet;
}  // namespace extensions

namespace gfx {
class ImageSkia;
}

// Displays all the UI around extension installation.
class ExtensionInstallPrompt {
 public:
  // This enum is associated with Extensions.InstallPrompt_Type UMA histogram.
  // Do not modify existing values and add new values only to the end.
  enum PromptType {
    UNSET_PROMPT_TYPE = -1,
    INSTALL_PROMPT = 0,
    // INLINE_INSTALL_PROMPT_DEPRECATED = 1,
    // BUNDLE_INSTALL_PROMPT_DEPRECATED = 2,
    RE_ENABLE_PROMPT = 3,
    PERMISSIONS_PROMPT = 4,
    EXTERNAL_INSTALL_PROMPT = 5,
    // POST_INSTALL_PERMISSIONS_PROMPT_DEPRECATED = 6,
    // LAUNCH_PROMPT_DEPRECATED = 7,
    REMOTE_INSTALL_PROMPT = 8,
    REPAIR_PROMPT = 9,
    // DELEGATED_PERMISSIONS_PROMPT = 10,
    // DELEGATED_BUNDLE_PERMISSIONS_PROMPT_DEPRECATED = 11,
    // WEBSTORE_WIDGET_PROMPT_DEPRECATED = 12,
    EXTENSION_REQUEST_PROMPT = 13,
    EXTENSION_PENDING_REQUEST_PROMPT = 14,
    NUM_PROMPT_TYPES = 15,
    // WAIT! Are you adding a new prompt type? Does it *install an extension*?
    // If not, please create a new dialog, rather than adding more functionality
    // to this class - it's already too full.
  };

  // The last prompt type to display; only used for testing.
  static PromptType g_last_prompt_type_for_tests;

  // Interface for observing events on the prompt.
  class Observer : public base::CheckedObserver {
   public:
    // Called right before the dialog is about to show.
    virtual void OnDialogOpened() = 0;

    // Called when the user clicks accept on the dialog.
    virtual void OnDialogAccepted() = 0;

    // Called when the user clicks cancel on the dialog, presses 'x' or escape.
    virtual void OnDialogCanceled() = 0;
  };

  // Extra information needed to display an installation or uninstallation
  // prompt. Gets populated with raw data and exposes getters for formatted
  // strings so that the GTK/views/Cocoa install dialogs don't have to repeat
  // that logic.
  class Prompt {
   public:
    explicit Prompt(PromptType type);

    Prompt(const Prompt&) = delete;
    Prompt& operator=(const Prompt&) = delete;

    ~Prompt();

    void AddPermissionSet(const extensions::PermissionSet& permissions);
    void AddPermissionMessages(
        const extensions::PermissionMessages& permissions);
    void SetWebstoreData(const std::string& localized_user_count,
                         bool show_user_count,
                         double average_rating,
                         int rating_count,
                         const std::string& localized_rating_count);

    PromptType type() const { return type_; }

    // Getters for UI element labels.
    std::u16string GetDialogTitle() const;
    int GetDialogButtons() const;
    // Returns the empty string when there should be no "accept" button.
    std::u16string GetAcceptButtonLabel() const;
    std::u16string GetAbortButtonLabel() const;
    std::u16string GetPermissionsHeading() const;

    void set_requires_parent_permission(bool requires_parent_permission) {
      requires_parent_permission_ = requires_parent_permission;
    }

    bool requires_parent_permission() const {
      return requires_parent_permission_;
    }

    // Returns whether the dialog should withheld permissions if the dialog is
    // accepted.
    bool ShouldWithheldPermissionsOnDialogAccept() const;

    // Getters for webstore metadata. Only populated when the type is
    // INLINE_INSTALL_PROMPT, EXTERNAL_INSTALL_PROMPT, or REPAIR_PROMPT.

    // The star display logic replicates the one used by the webstore (from
    // components.ratingutils.setFractionalYellowStars). Callers pass in an
    // "appender", which will be repeatedly called back with the star images
    // that they append to the star display area.
    typedef void(*StarAppender)(const gfx::ImageSkia*, void*);
    void AppendRatingStars(StarAppender appender, void* data) const;
    std::u16string GetRatingCount() const;
    std::u16string GetUserCount() const;
    size_t GetPermissionCount() const;
    std::u16string GetPermission(size_t index) const;
    std::u16string GetPermissionsDetails(size_t index) const;

    const extensions::Extension* extension() const { return extension_; }
    void set_extension(const extensions::Extension* extension) {
      extension_ = extension;
    }

    const gfx::Image& icon() const { return icon_; }
    void set_icon(const gfx::Image& icon) { icon_ = icon; }

    double average_rating() const { return average_rating_; }
    int rating_count() const { return rating_count_; }
    const std::string& localized_rating_count() const {
      return localized_rating_count_;
    }

    bool has_webstore_data() const { return has_webstore_data_; }

    void AddObserver(Observer* observer);
    void RemoveObserver(Observer* observer);

    // Called right before the dialog is about to show.
    void OnDialogOpened();

    // Called when the user clicks accept on the dialog.
    void OnDialogAccepted();

    // Called when the user clicks cancel on the dialog, presses 'x' or escape.
    void OnDialogCanceled();

   private:
    const PromptType type_;

    // Permissions that are being requested (may not be all of an extension's
    // permissions if only additional ones are being requested)
    extensions::InstallPromptPermissions prompt_permissions_;

    // True if the current user is a child.
    bool requires_parent_permission_ = false;

    bool is_requesting_host_permissions_;

    // The extension being installed.
    raw_ptr<const extensions::Extension, AcrossTasksDanglingUntriaged>
        extension_;

    // The icon to be displayed.
    gfx::Image icon_;

    // These fields are populated only when the prompt type is
    // INLINE_INSTALL_PROMPT
    // Already formatted to be locale-specific.
    std::string localized_user_count_;
    // Range is kMinExtensionRating to kMaxExtensionRating
    double average_rating_;
    // The rating count for the extension, used for string pluralization.
    int rating_count_;
    // The localized rating count for the extension, used as-is for display.
    std::string localized_rating_count_;

    // Whether we should display the user count (we anticipate this will be
    // false if localized_user_count_ represents the number zero).
    bool show_user_count_;

    // Whether or not this prompt has been populated with data from the
    // webstore.
    bool has_webstore_data_;

    std::vector<base::FilePath> retained_files_;
    std::vector<std::u16string> retained_device_messages_;

    base::ObserverList<Observer> observers_;
  };

  static const int kMinExtensionRating = 0;
  static const int kMaxExtensionRating = 5;

  enum class Result {
    ACCEPTED,
    ACCEPTED_WITH_WITHHELD_PERMISSIONS,
    USER_CANCELED,
    ABORTED,
  };

  struct DoneCallbackPayload {
    explicit DoneCallbackPayload(Result result);
    DoneCallbackPayload(Result result, std::string justification);
    ~DoneCallbackPayload() = default;

    const Result result;
    const std::string justification;
  };

  using DoneCallback = base::OnceCallback<void(DoneCallbackPayload payload)>;

  using ShowDialogCallback = base::RepeatingCallback<void(
      std::unique_ptr<ExtensionInstallPromptShowParams>,
      DoneCallback,
      std::unique_ptr<ExtensionInstallPrompt::Prompt>)>;

  // Callback to show the default extension install dialog.
  // The implementations of this function are platform-specific.
  static ShowDialogCallback GetDefaultShowDialogCallback();

  // Returns the appropriate prompt type for the given |extension|.
  // TODO(devlin): This method is yucky - callers probably only care about one
  // prompt type. We just need to comb through and figure out what it is.
  static PromptType GetReEnablePromptTypeForExtension(
      content::BrowserContext* context,
      const extensions::Extension* extension);

  // Creates a dummy extension from the |manifest|, replacing the name and
  // description with the localizations if provided.
  static scoped_refptr<extensions::Extension> GetLocalizedExtensionForDisplay(
      const base::Value::Dict& manifest,
      int flags,  // Extension::InitFromValueFlags
      const std::string& id,
      const std::string& localized_name,
      const std::string& localized_description,
      std::string* error);

  // Creates a prompt with a parent web content.
  explicit ExtensionInstallPrompt(content::WebContents* contents);

  // Creates a prompt with a profile and a native window. The most recently
  // active browser window (or a new browser window if there are no browser
  // windows) is used if a new tab needs to be opened.
  ExtensionInstallPrompt(Profile* profile, gfx::NativeWindow native_window);

  ExtensionInstallPrompt(const ExtensionInstallPrompt&) = delete;
  ExtensionInstallPrompt& operator=(const ExtensionInstallPrompt&) = delete;

  virtual ~ExtensionInstallPrompt();

  ExtensionInstallUI* install_ui() const { return install_ui_.get(); }

  // Starts the process to show the install dialog. Loads the icon (if |icon| is
  // null), sets up the Prompt, and calls |show_dialog_callback| when ready to
  // show.
  // |extension| can be null in the case of a bndle install.
  // If |icon| is null, this will attempt to load the extension's icon.
  // |prompt| is used to pass in a prompt with additional data (like retained
  // device permissions) or a different type. If not provided, |prompt| will
  // be created as an INSTALL_PROMPT.
  // |custom_permissions| will be used if provided; otherwise, the extensions
  // current permissions are used.
  //
  // The |install_callback| *MUST* eventually be called.
  void ShowDialog(DoneCallback install_callback,
                  const extensions::Extension* extension,
                  const SkBitmap* icon,
                  const ShowDialogCallback& show_dialog_callback);
  void ShowDialog(DoneCallback install_callback,
                  const extensions::Extension* extension,
                  const SkBitmap* icon,
                  std::unique_ptr<Prompt> prompt,
                  const ShowDialogCallback& show_dialog_callback);
  // Declared virtual for testing purposes.
  // Note: if all you want to do is automatically confirm or cancel, prefer
  // ScopedTestDialogAutoConfirm from extension_dialog_auto_confirm.h
  virtual void ShowDialog(
      DoneCallback install_callback,
      const extensions::Extension* extension,
      const SkBitmap* icon,
      std::unique_ptr<Prompt> prompt,
      std::unique_ptr<const extensions::PermissionSet> custom_permissions,
      const ShowDialogCallback& show_dialog_callback);

  // Installation was successful. This is declared virtual for testing.
  virtual void OnInstallSuccess(
      scoped_refptr<const extensions::Extension> extension,
      SkBitmap* icon);

  // Installation failed. This is declared virtual for testing.
  virtual void OnInstallFailure(const extensions::CrxInstallError& error);

  bool did_call_show_dialog() const { return did_call_show_dialog_; }

  std::unique_ptr<Prompt> GetPromptForTesting();

 private:
  // Sets the icon that will be used in any UI. If |icon| is NULL, or contains
  // an empty bitmap, then a default icon will be used instead.
  void SetIcon(const SkBitmap* icon);

  // ImageLoader callback.
  void OnImageLoaded(const gfx::Image& image);

  // Starts the process of showing a confirmation UI, which is split into two.
  // 1) Set off a 'load icon' task.
  // 2) Handle the load icon response and show the UI (OnImageLoaded).
  void LoadImageIfNeeded();

  // Shows the actual UI (the icon should already be loaded).
  void ShowConfirmation();

  // If auto confirm is enabled then posts a task to proceed with or cancel the
  // install and returns true. Otherwise returns false.
  bool AutoConfirmPromptIfEnabled();

  raw_ptr<Profile, DanglingUntriaged> profile_;

  base::ThreadChecker ui_thread_checker_;

  // The extensions installation icon.
  SkBitmap icon_;

  // The extension we are showing the UI for.
  scoped_refptr<const extensions::Extension> extension_;

  // A custom set of permissions to show in the install prompt instead of the
  // extension's active permissions.
  std::unique_ptr<const extensions::PermissionSet> custom_permissions_;

  // The object responsible for doing the UI specific actions.
  std::unique_ptr<ExtensionInstallUI> install_ui_;

  // Parameters to show the confirmation UI.
  std::unique_ptr<ExtensionInstallPromptShowParams> show_params_;

  // The callback to run with the result.
  DoneCallback done_callback_;

  // A pre-filled prompt.
  std::unique_ptr<Prompt> prompt_;

  // Used to show the confirm dialog.
  ShowDialogCallback show_dialog_callback_;

  // Whether or not the |show_dialog_callback_| was called.
  bool did_call_show_dialog_;

  base::WeakPtrFactory<ExtensionInstallPrompt> weak_factory_{this};
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_PROMPT_H_
