// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_CONSENT_PROVIDER_IMPL_H_
#define CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_CONSENT_PROVIDER_IMPL_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "extensions/browser/api/file_system/consent_provider.h"
#include "extensions/common/extension_id.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/ui_base_types.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace extensions {
class Extension;
class ScopedSkipRequestFileSystemDialog;

namespace file_system_api {

// Requests consent for the chrome.fileSystem.requestFileSystem() method.
// Interaction with UI and environmental checks (kiosk mode, allowlist) are
// provided by a delegate: ConsentProviderDelegate. For testing, it is
// TestingConsentProviderDelegate.
// This class may post callbacks given to it, but does not asynchronously call
// itself. It is generally safe to use a temporary ConsentProviderImpl.
// TODO(crbug.com/40234505): Make this easier to use, perhaps by replacing
// member functions with static methods.
class ConsentProviderImpl : public ConsentProvider {
 public:
  using ShowDialogCallback = base::OnceCallback<void(ui::mojom::DialogButton)>;

  // Interface for delegating user interaction for granting permissions.
  class DelegateInterface {
   public:
    DelegateInterface();
    virtual ~DelegateInterface();

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

  explicit ConsentProviderImpl(std::unique_ptr<DelegateInterface> delegate);

  ConsentProviderImpl(const ConsentProviderImpl&) = delete;
  ConsentProviderImpl& operator=(const ConsentProviderImpl&) = delete;

  ~ConsentProviderImpl() override;

  // ConsentProvider:
  void RequestConsent(content::RenderFrameHost* host,
                      const Extension& extension,
                      const std::string& volume_id,
                      const std::string& volume_label,
                      bool writable,
                      ConsentCallback callback) override;
  bool IsGrantable(const Extension& extension) override;

 private:
  std::unique_ptr<DelegateInterface> delegate_;
};

// Handles interaction with user as well as environment checks (allowlists,
// context of running extensions) for ConsentProviderImpl. The class is used
// during async calls (in particular, crosapi for Lacros), and the |profile_|
// provided may disappear. To handle this, the class observes |profile_|
// destruction, and properly disables calls that need |profile_|.
class ConsentProviderDelegate : public ConsentProviderImpl::DelegateInterface,
                                public ProfileObserver {
 public:
  explicit ConsentProviderDelegate(Profile* profile);

  ConsentProviderDelegate(const ConsentProviderDelegate&) = delete;
  ConsentProviderDelegate& operator=(const ConsentProviderDelegate&) = delete;

  ~ConsentProviderDelegate() override;

 private:
  friend ScopedSkipRequestFileSystemDialog;

  // Sets a fake result for the user consent dialog. If
  // ui::mojom::DialogButton::kNone then disabled.
  static void SetAutoDialogButtonForTest(ui::mojom::DialogButton button);

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

  // ConsentProviderImpl::DelegateInterface overrides:
  void ShowDialog(content::RenderFrameHost* host,
                  const extensions::ExtensionId& extension_id,
                  const std::string& extension_name,
                  const std::string& volume_id,
                  const std::string& volume_label,
                  bool writable,
                  ConsentProviderImpl::ShowDialogCallback callback) override;
  void ShowNotification(const extensions::ExtensionId& extension_id,
                        const std::string& extension_name,
                        const std::string& volume_id,
                        const std::string& volume_label,
                        bool writable) override;
  bool IsAutoLaunched(const Extension& extension) override;
  bool IsAllowlistedComponent(const Extension& extension) override;

  // |profile_| can be a raw pointer since its destruction is observed.
  raw_ptr<Profile> profile_;
  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};
};

}  // namespace file_system_api
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_CONSENT_PROVIDER_IMPL_H_
