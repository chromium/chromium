// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_PROMPT_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_PROMPT_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "extensions/common/permissions/permission_message.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/native_widget_types.h"

class ExtensionInstallPromptShowParams;
class Profile;

namespace base {
class DictionaryValue;
}  // namespace base

namespace content {
class BrowserContext;
class WebContents;
}

namespace extensions {
class CrxInstallError;
class Extension;
class ExtensionInstallUI;
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
    POST_INSTALL_PERMISSIONS_PROMPT = 6,
    // LAUNCH_PROMPT_DEPRECATED = 7,
    REMOTE_INSTALL_PROMPT = 8,
    REPAIR_PROMPT = 9,
    DELEGATED_PERMISSIONS_PROMPT = 10,
    // DELEGATED_BUNDLE_PERMISSIONS_PROMPT_DEPRECATED = 11,
    WEBSTORE_WIDGET_PROMPT = 12,
    NUM_PROMPT_TYPES = 13,
  };

  // The last prompt type to display; only used for testing.
  static PromptType g_last_prompt_type_for_tests;

  // Enumeration for permissions and retained files details.
  enum DetailsType {
    PERMISSIONS_DETAILS = 0,
    RETAINED_FILES_DETAILS,
    RETAINED_DEVICES_DETAILS,
  };

  // Extra information needed to display an installation or uninstallation
  // prompt. Gets populated with raw data and exposes getters for formatted
  // strings so that the GTK/views/Cocoa install dialogs don't have to repeat
  // that logic.
  class Prompt {
   public:
    explicit Prompt(PromptType type);
    ~Prompt();

    void AddPermissions(const extensions::PermissionMessages& permissions);
    void SetIsShowingDetails(DetailsType type,
                             size_t index,
                             bool is_showing_details);
    void SetWebstoreData(const std::string& localized_user_count,
                         bool show_user_count,
                         double average_rating,
                         int rating_count);

    PromptType type() const { return type_; }

    // Getters for UI element labels.
    base::string16 GetDialogTitle() const;
    int GetDialogButtons() const;
    // Returns the empty string when there should be no "accept" button.
    base::string16 GetAcceptButtonLabel() const;
    base::string16 GetAbortButtonLabel() const;
    base::string16 GetPermissionsHeading() const;
    base::string16 GetRetainedFilesHeading() const;
    base::string16 GetRetainedDevicesHeading() const;

    bool ShouldShowPermissions() const;

    // Getters for webstore metadata. Only populated when the type is
    // INLINE_INSTALL_PROMPT, EXTERNAL_INSTALL_PROMPT, or REPAIR_PROMPT.

    // The star display logic replicates the one used by the webstore (from
    // components.ratingutils.setFractionalYellowStars). Callers pass in an
    // "appender", which will be repeatedly called back with the star images
    // that they append to the star display area.
    typedef void(*StarAppender)(const gfx::ImageSkia*, void*);
    void AppendRatingStars(StarAppender appender, void* data) const;
    base::string16 GetRatingCount() const;
    base::string16 GetUserCount() const;
    size_t GetPermissionCount() const;
    size_t GetPermissionsDetailsCount() const;
    base::string16 GetPermission(size_t index) const;
    base::string16 GetPermissionsDetails(size_t index) const;
    bool GetIsShowingDetails(DetailsType type, size_t index) const;
    size_t GetRetainedFileCount() const;
    base::string16 GetRetainedFile(size_t index) const;
    size_t GetRetainedDeviceCount() const;
    base::string16 GetRetainedDeviceMessageString(size_t index) const;

    const extensions::Extension* extension() const { return extension_; }
    void set_extension(const extensions::Extension* extension) {
      extension_ = extension;
    }

    // May be populated for POST_INSTALL_PERMISSIONS_PROMPT.
    void set_retained_files(const std::vector<base::FilePath>& retained_files) {
      retained_files_ = retained_files;
    }
    void set_retained_device_messages(
        const std::vector<base::string16>& retained_device_messages) {
      retained_device_messages_ = retained_device_messages;
    }

    const std::string& delegated_username() const {
      return delegated_username_;
    }
    void set_delegated_username(const std::string& delegated_username) {
      delegated_username_ = delegated_username;
    }

    const gfx::Image& icon() const { return icon_; }
    void set_icon(const gfx::Image& icon) { icon_ = icon; }

    double average_rating() const { return average_rating_; }
    int rating_count() const { return rating_count_; }

    bool has_webstore_data() const { return has_webstore_data_; }

   private:
    friend class base::RefCountedThreadSafe<Prompt>;

    struct InstallPromptPermissions {
      InstallPromptPermissions();
      ~InstallPromptPermissions();

      std::vector<base::string16> permissions;
      std::vector<base::string16> details;
      std::vector<bool> is_showing_details;
    };

    bool ShouldDisplayRevokeButton() const;

    bool ShouldDisplayRevokeFilesButton() const;

    const PromptType type_;

    // Permissions that are being requested (may not be all of an extension's
    // permissions if only additional ones are being requested)
    InstallPromptPermissions prompt_permissions_;

    bool is_showing_details_for_retained_files_;
    bool is_showing_details_for_retained_devices_;

    // The extension being installed.
    const extensions::Extension* extension_;

    std::string delegated_username_;

    // The icon to be displayed.
    gfx::Image icon_;

    // These fields are populated only when the prompt type is
    // INLINE_INSTALL_PROMPT
    // Already formatted to be locale-specific.
    std::string localized_user_count_;
    // Range is kMinExtensionRating to kMaxExtensionRating
    double average_rating_;
    int rating_count_;

    // Whether we should display the user count (we anticipate this will be
    // false if localized_user_count_ represents the number zero).
    bool show_user_count_;

    // Whether or not this prompt has been populated with data from the
    // webstore.
    bool has_webstore_data_;

    std::vector<base::FilePath> retained_files_;
    std::vector<base::string16> retained_device_messages_;

    DISALLOW_COPY_AND_ASSIGN(Prompt);
  };

  static const int kMinExtensionRating = 0;
  static const int kMaxExtensionRating = 5;

  enum class Result {
    ACCEPTED,
    USER_CANCELED,
    ABORTED,
  };

  using DoneCallback = base::Callback<void(Result result)>;

  typedef base::Callback<void(ExtensionInstallPromptShowParams*,
                              const DoneCallback&,
                              std::unique_ptr<ExtensionInstallPrompt::Prompt>)>
      ShowDialogCallback;

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
      const base::DictionaryValue* manifest,
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

  virtual ~ExtensionInstallPrompt();

  extensions::ExtensionInstallUI* install_ui() const {
    return install_ui_.get();
  }

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
  void ShowDialog(const DoneCallback& install_callback,
                  const extensions::Extension* extension,
                  const SkBitmap* icon,
                  const ShowDialogCallback& show_dialog_callback);
  void ShowDialog(const DoneCallback& install_callback,
                  const extensions::Extension* extension,
                  const SkBitmap* icon,
                  std::unique_ptr<Prompt> prompt,
                  const ShowDialogCallback& show_dialog_callback);
  // Declared virtual for testing purposes.
  // Note: if all you want to do is automatically confirm or cancel, prefer
  // ScopedTestDialogAutoConfirm from extension_dialog_auto_confirm.h
  virtual void ShowDialog(
      const DoneCallback& install_callback,
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

  Profile* profile_;

  base::ThreadChecker ui_thread_checker_;

  // The extensions installation icon.
  SkBitmap icon_;

  // The extension we are showing the UI for.
  scoped_refptr<const extensions::Extension> extension_;

  // A custom set of permissions to show in the install prompt instead of the
  // extension's active permissions.
  std::unique_ptr<const extensions::PermissionSet> custom_permissions_;

  // The object responsible for doing the UI specific actions.
  std::unique_ptr<extensions::ExtensionInstallUI> install_ui_;

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

  DISALLOW_COPY_AND_ASSIGN(ExtensionInstallPrompt);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_PROMPT_H_
