// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_CONSENT_PROVIDER_H_
#define CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_CONSENT_PROVIDER_H_

#include <string>

#include "base/callback_forward.h"
#include "extensions/common/extension_id.h"
#include "ui/base/ui_base_types.h"

class Profile;

namespace content {
class RenderFrameHost;
}  // content

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
    virtual void ShowDialog(content::RenderFrameHost* host,
                            const extensions::ExtensionId& extension_id,
                            const std::string& extension_name,
                            const std::string& volume_id,
                            const std::string& volume_label,
                            bool writable,
                            ShowDialogCallback callback) = 0;

    // Shows a notification about permissions automatically granted access.
    virtual void ShowNotification(const extensions::ExtensionId& extension_id,
                                  const std::string& extension_name,
                                  const std::string& volume_id,
                                  const std::string& volume_label,
                                  bool writable) = 0;

    // Checks if the extension was launched in auto-launch kiosk mode.
    virtual bool IsAutoLaunched(const Extension& extension) = 0;

    // Checks if the extension is a allowlisted component extension or app.
    virtual bool IsAllowlistedComponent(const Extension& extension) = 0;
  };

  explicit ConsentProvider(DelegateInterface* delegate);

  ConsentProvider(const ConsentProvider&) = delete;
  ConsentProvider& operator=(const ConsentProvider&) = delete;

  ~ConsentProvider();

  // Requests consent for granting |writable| permissions to a volume with
  // |volume_id| and |volume_label| by |extension|, which is assumed to be
  // grantable (i.e., passes IsGrantable()).
  void RequestConsent(content::RenderFrameHost* host,
                      const Extension& extension,
                      const std::string& volume_id,
                      const std::string& volume_label,
                      bool writable,
                      ConsentCallback callback);

  // Checks whether the |extension| can be granted access.
  bool IsGrantable(const Extension& extension);

 private:
  DelegateInterface* const delegate_;
};

// Handles interaction with user as well as environment checks (allowlists,
// context of running extensions) for ConsentProvider.
class ConsentProviderDelegate : public ConsentProvider::DelegateInterface {
 public:
  explicit ConsentProviderDelegate(Profile* profile);

  ConsentProviderDelegate(const ConsentProviderDelegate&) = delete;
  ConsentProviderDelegate& operator=(const ConsentProviderDelegate&) = delete;

  ~ConsentProviderDelegate();

 private:
  friend ScopedSkipRequestFileSystemDialog;

  // Sets a fake result for the user consent dialog. If ui::DIALOG_BUTTON_NONE
  // then disabled.
  static void SetAutoDialogButtonForTest(ui::DialogButton button);

  // ConsentProvider::DelegateInterface overrides:
  void ShowDialog(content::RenderFrameHost* host,
                  const extensions::ExtensionId& extension_id,
                  const std::string& extension_name,
                  const std::string& volume_id,
                  const std::string& volume_label,
                  bool writable,
                  ConsentProvider::ShowDialogCallback callback) override;

  void ShowNotification(const extensions::ExtensionId& extension_id,
                        const std::string& extension_name,
                        const std::string& volume_id,
                        const std::string& volume_label,
                        bool writable) override;
  bool IsAutoLaunched(const Extension& extension) override;
  bool IsAllowlistedComponent(const Extension& extension) override;

  Profile* const profile_;
};

}  // namespace file_system_api
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_CONSENT_PROVIDER_H_
