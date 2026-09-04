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
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/values.h"
#include "chrome/browser/ui/extensions/extension_install_ui.h"
#include "extensions/browser/extension_install_prompt_client.h"
#include "extensions/browser/install_prompt_data.h"
#include "extensions/buildflags/buildflags.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/native_ui_types.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

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

// Displays all the UI around extension installation.
class ExtensionInstallPrompt : public extensions::ExtensionInstallPromptClient {
 public:
  // The last prompt type to display; only used for testing.
  static extensions::InstallPromptData::PromptType g_last_prompt_type_for_tests;

  using DoneCallback = extensions::ExtensionInstallPromptClient::DoneCallback;
  using DoneCallbackPayload =
      extensions::ExtensionInstallPromptClient::DoneCallbackPayload;

  using ShowDialogCallback = base::RepeatingCallback<void(
      std::unique_ptr<ExtensionInstallPromptShowParams>,
      DoneCallback,
      std::unique_ptr<extensions::InstallPromptData>)>;

  // Callback to show the default extension install dialog.
  // The implementations of this function are platform-specific.
  static ShowDialogCallback GetDefaultShowDialogCallback();

  // Returns the appropriate prompt type for the given `extension`.
  // TODO(devlin): This method is yucky - callers probably only care about one
  // prompt type. We just need to comb through and figure out what it is.
  static extensions::InstallPromptData::PromptType
  GetReEnablePromptTypeForExtension(content::BrowserContext* context,
                                    const extensions::Extension* extension);

  // Creates a prompt with a parent web content and a prompt data.
  explicit ExtensionInstallPrompt(
      content::WebContents* contents,
      std::unique_ptr<extensions::InstallPromptData> prompt);

  // Creates a prompt with a profile and a native window. The most recently
  // active browser window (or a new browser window if there are no browser
  // windows) is used if a new tab needs to be opened.
  ExtensionInstallPrompt(Profile* profile,
                         gfx::NativeWindow native_window,
                         std::unique_ptr<extensions::InstallPromptData> prompt);

  ExtensionInstallPrompt(const ExtensionInstallPrompt&) = delete;
  ExtensionInstallPrompt& operator=(const ExtensionInstallPrompt&) = delete;

  ~ExtensionInstallPrompt() override;

  ExtensionInstallUI* install_ui() const { return install_ui_.get(); }

  // Starts the process to show the install dialog. Loads the icon (if `icon` is
  // null), sets up the InstallPromptData, and calls `show_dialog_callback` when
  // ready to show. `extension` can be null in the case of a bundle install. If
  // `icon` is null, this will attempt to load the extension's icon.
  // `custom_permissions` will be used if provided; otherwise, the extension's
  // current permissions are used. The prompt type and additional data should be
  // specified in the constructor.
  //
  // The `install_callback` *MUST* eventually be called.
  void ShowDialog(DoneCallback install_callback,
                  const extensions::Extension* extension,
                  const SkBitmap* icon,
                  const ShowDialogCallback& show_dialog_callback);
  void ShowDialog(
      DoneCallback install_callback,
      const extensions::Extension* extension,
      const SkBitmap* icon,
      std::unique_ptr<const extensions::PermissionSet> custom_permissions,
      const ShowDialogCallback& show_dialog_callback);

  // extensions::ExtensionInstallPromptClient:
  void OnInstallSuccess(scoped_refptr<const extensions::Extension> extension,
                        SkBitmap* icon) override;
  void OnInstallFailure(const extensions::CrxInstallError& error) override;
  void SetUseAppInstalledBubble(bool use_bubble) override;
  void SetSkipPostInstallUI(bool skip_ui) override;
  void ConfirmInstall(DoneCallback install_callback,
                      const extensions::Extension* extension) override;
  void ConfirmReEnable(DoneCallback install_callback,
                       const extensions::Extension* extension,
                       content::BrowserContext* browser_context) override;
  void ConfirmPermissions(DoneCallback done_callback,
                          const extensions::Extension* extension,
                          std::unique_ptr<const extensions::PermissionSet>
                              custom_permissions) override;
  void ShowInstallDialog(DoneCallback install_callback,
                         const extensions::Extension* extension,
                         const SkBitmap* icon) override;

  bool did_call_show_dialog() const { return did_call_show_dialog_; }

  std::unique_ptr<extensions::InstallPromptData> GetPromptForTesting();

 private:
  // Sets the icon that will be used in any UI. If `icon` is NULL, or contains
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
  std::unique_ptr<extensions::InstallPromptData> prompt_;

  // Used to show the confirm dialog.
  ShowDialogCallback show_dialog_callback_;

  // Whether or not the `show_dialog_callback_` was called.
  bool did_call_show_dialog_;

  base::WeakPtrFactory<ExtensionInstallPrompt> weak_factory_{this};
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_PROMPT_H_
