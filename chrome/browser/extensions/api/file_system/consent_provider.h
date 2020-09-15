// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_CONSENT_PROVIDER_H_
#define CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_CONSENT_PROVIDER_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "extensions/browser/api/file_system/file_system_delegate.h"
#include "ui/base/ui_base_types.h"

class Profile;

namespace content {
class RenderFrameHost;
}  // content

namespace file_manager {
class Volume;
}  // namespace file_manager

namespace extensions {
class Extension;
class ScopedSkipRequestFileSystemDialog;

namespace file_system_api {

// Requests consent for the chrome.fileSystem.requestFileSystem() method.
// Interaction with UI and environmental checks (kiosk mode, allowlist) are
// provided by a delegate: ConsentProviderDelegate. For testing, it is
// TestingConsentProviderDelegate.
// This class may post callbacks given to it, but does not asynchronously call
// itself. It is generally safe to use a temporary ConsentProvider.
// TODO(michaelpg): Make this easier to use by replacing member functions with
// static methods.
class ConsentProvider {
 public:
  enum Consent { CONSENT_GRANTED, CONSENT_REJECTED, CONSENT_IMPOSSIBLE };
  using ConsentCallback = base::OnceCallback<void(Consent)>;
  using ShowDialogCallback = base::OnceCallback<void(ui::DialogButton)>;

  // Interface for delegating user interaction for granting permissions.
  class DelegateInterface {
   public:
    // Shows a dialog for granting permissions.
    virtual void ShowDialog(const Extension& extension,
                            content::RenderFrameHost* host,
                            const base::WeakPtr<file_manager::Volume>& volume,
                            bool writable,
                            ShowDialogCallback callback) = 0;

    // Shows a notification about permissions automatically granted access.
    virtual void ShowNotification(
        const Extension& extension,
        const base::WeakPtr<file_manager::Volume>& volume,
        bool writable) = 0;

    // Checks if the extension was launched in auto-launch kiosk mode.
    virtual bool IsAutoLaunched(const Extension& extension) = 0;

    // Checks if the extension is a allowlisted component extension or app.
    virtual bool IsAllowlistedComponent(const Extension& extension) = 0;

    // Checks if the extension has the permission to access Downloads.
    virtual bool HasRequestDownloadsPermission(const Extension& extension) = 0;
  };

  explicit ConsentProvider(DelegateInterface* delegate);
  ~ConsentProvider();

  // Requests consent for granting |writable| permissions to the |volume|
  // volume by the |extension|. Must be called only if the extension is
  // grantable, which can be checked with GetGrantVolumesMode() and
  // IsGrantableForVolume().
  void RequestConsent(const Extension& extension,
                      content::RenderFrameHost* host,
                      const base::WeakPtr<file_manager::Volume>& volume,
                      bool writable,
                      ConsentCallback callback);

  // Returns granted access mode for the |extension|.
  FileSystemDelegate::GrantVolumesMode GetGrantVolumesMode(
      const Extension& extension);

  // Checks whether the |extension| can be granted |volume| access.
  bool IsGrantableForVolume(const Extension& extension,
                            const base::WeakPtr<file_manager::Volume>& volume);

 private:
  DelegateInterface* const delegate_;

  DISALLOW_COPY_AND_ASSIGN(ConsentProvider);
};

// Handles interaction with user as well as environment checks (allowlists,
// context of running extensions) for ConsentProvider.
class ConsentProviderDelegate : public ConsentProvider::DelegateInterface {
 public:
  explicit ConsentProviderDelegate(Profile* profile);
  ~ConsentProviderDelegate();

 private:
  friend ScopedSkipRequestFileSystemDialog;

  // Sets a fake result for the user consent dialog. If ui::DIALOG_BUTTON_NONE
  // then disabled.
  static void SetAutoDialogButtonForTest(ui::DialogButton button);

  // ConsentProvider::DelegateInterface overrides:
  void ShowDialog(
      const Extension& extension,
      content::RenderFrameHost* host,
      const base::WeakPtr<file_manager::Volume>& volume,
      bool writable,
      file_system_api::ConsentProvider::ShowDialogCallback callback) override;
  void ShowNotification(const Extension& extension,
                        const base::WeakPtr<file_manager::Volume>& volume,
                        bool writable) override;
  bool IsAutoLaunched(const Extension& extension) override;
  bool IsAllowlistedComponent(const Extension& extension) override;
  bool HasRequestDownloadsPermission(const Extension& extension) override;

  Profile* const profile_;

  DISALLOW_COPY_AND_ASSIGN(ConsentProviderDelegate);
};

}  // namespace file_system_api
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_CONSENT_PROVIDER_H_
