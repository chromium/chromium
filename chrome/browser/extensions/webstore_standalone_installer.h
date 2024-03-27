// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_WEBSTORE_STANDALONE_INSTALLER_H_
#define CHROME_BROWSER_EXTENSIONS_WEBSTORE_STANDALONE_INSTALLER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/extensions/active_install_data.h"
#include "chrome/browser/extensions/cws_item_service.pb.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/webstore_data_fetcher_delegate.h"
#include "chrome/browser/extensions/webstore_install_helper.h"
#include "chrome/browser/extensions/webstore_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/common/extensions/webstore_install_result.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace extensions {
class Extension;
class ScopedActiveInstall;
class WebstoreDataFetcher;

// A a purely abstract base for concrete classes implementing various types of
// standalone installs:
// 1) Downloads and parses metadata from the webstore.
// 2) Optionally shows an install dialog.
// 3) Starts download once the user confirms (if confirmation was requested).
// 4) Optionally shows a post-install UI.
// Follows the Template Method pattern. Implementing subclasses must override
// the primitive hooks in the corresponding section below.

class WebstoreStandaloneInstaller
    : public base::RefCountedThreadSafe<WebstoreStandaloneInstaller>,
      public WebstoreDataFetcherDelegate,
      public WebstoreInstallHelper::Delegate,
      public ProfileObserver {
 public:
  // A callback for when the install process completes, successfully or not. If
  // there was a failure, |success| will be false and |error| may contain a
  // developer-readable error message about why it failed.
  using Callback = base::OnceCallback<void(bool success,
                                           const std::string& error,
                                           webstore_install::Result result)>;

  WebstoreStandaloneInstaller(const std::string& webstore_item_id,
                              Profile* profile,
                              Callback callback);

  WebstoreStandaloneInstaller(const WebstoreStandaloneInstaller&) = delete;
  WebstoreStandaloneInstaller& operator=(const WebstoreStandaloneInstaller&) =
      delete;

  void BeginInstall();

 protected:
  ~WebstoreStandaloneInstaller() override;

  // Runs the callback; primarily used for running a callback before it is
  // cleared in AbortInstall(). This should only be called once for the lifetime
  // of the class.
  void RunCallback(
      bool success, const std::string& error, webstore_install::Result result);

  // Called when the install should be aborted. The callback is cleared.
  void AbortInstall();

  // Checks InstallTracker and returns true if the same extension is not
  // currently being installed. Registers this install with the InstallTracker.
  bool EnsureUniqueInstall(webstore_install::Result* reason,
                           std::string* error);

  // Called when the install is complete.
  virtual void CompleteInstall(webstore_install::Result result,
                               const std::string& error);

  // Called when the installer should proceed to prompt the user.
  void ProceedWithInstallPrompt();

  // Lazily creates a dummy extension for display from the parsed manifest. This
  // is safe to call from OnManifestParsed() onwards. The manifest may be
  // invalid, thus the caller must check that the return value is not NULL.
  scoped_refptr<const Extension> GetLocalizedExtensionForDisplay();

  // Template Method's hooks to be implemented by subclasses.

  // Called at certain check points of the workflow to decide whether it makes
  // sense to proceed with installation. A requestor can be a website that
  // initiated an inline installation, or a command line option.
  virtual bool CheckRequestorAlive() const = 0;

  // Should a new tab be opened after installation to show the newly installed
  // extension's icon?
  virtual bool ShouldShowPostInstallUI() const = 0;

  // In the very least this should return a dummy WebContents (required
  // by some calls even when no prompt or other UI is shown). A non-dummy
  // WebContents is required if the prompt returned by CreateInstallPromt()
  // contains a navigable link(s). Returned WebContents should correspond
  // to |profile| passed into the constructor.
  virtual content::WebContents* GetWebContents() const = 0;

  // Should return an installation prompt with desired properties or NULL if
  // no prompt should be shown.
  virtual std::unique_ptr<ExtensionInstallPrompt::Prompt> CreateInstallPrompt()
      const = 0;

  // Will be called after the extension's manifest has been successfully parsed.
  // Subclasses can perform asynchronous checks at this point and call
  // ProceedWithInstallPrompt() to proceed with the install or otherwise call
  // CompleteInstall() with an error code. The default implementation calls
  // ProceedWithInstallPrompt().
  virtual void OnManifestParsed();

  // Returns an install UI to be shown. By default, this returns an install UI
  // that is a transient child of the host window for GetWebContents().
  virtual std::unique_ptr<ExtensionInstallPrompt> CreateInstallUI();

  // Create an approval to pass installation parameters to the CrxInstaller.
  virtual std::unique_ptr<WebstoreInstaller::Approval> CreateApproval() const;

  // Called once the install prompt has finished.
  virtual void OnInstallPromptDone(
      ExtensionInstallPrompt::DoneCallbackPayload payload);

  // Accessors to be used by subclasses.
  bool show_user_count() const { return show_user_count_; }
  const std::string& localized_user_count() const {
    return localized_user_count_;
  }
  double average_rating() const { return average_rating_; }
  int rating_count() const { return rating_count_; }
  const std::string& localized_rating_count() const {
    return localized_rating_count_;
  }
  void set_install_source(WebstoreInstaller::InstallSource source) {
    install_source_ = source;
  }
  WebstoreInstaller::InstallSource install_source() const {
    return install_source_;
  }
  Profile* profile() const { return profile_; }
  const std::string& id() const { return id_; }
  const base::Value::Dict& manifest() const { return manifest_.value(); }
  const Extension* localized_extension_for_display() const {
    return localized_extension_for_display_.get();
  }

 private:
  friend class base::RefCountedThreadSafe<WebstoreStandaloneInstaller>;

  // Several delegate/client interface implementations follow. The normal flow
  // (for successful installs) for the item JSON API is:
  //
  // 1. BeginInstall: starts the fetch of data from the webstore.
  // 2. WebstoreDataFetcher::OnSimpleLoaderComplete: starts the parsing of data
  //    from the webstore.
  // 3. OnWebstoreItemJSONAPIResponseParseSuccess: starts the parsing of the
  //    manifest and fetching of icon data.
  // 4. OnWebstoreParseSuccess: shows the install UI
  // 5. InstallUIProceed: initiates the .crx download/install
  //
  // For the new item snippets API, the flow is:
  // 1. BeginInstall: starts the fetch of data from the webstore.
  // 2. WebstoreDataFetcher::OnFetchItemSnippetResponseReceived: starts the
  //    parsing of data from the webstore into a FetchItemSnippetResponse
  //    protobuf.
  // 3. OnFetchItemSnippetParseSuccess: starts the parsing of the
  //    manifest and fetching of icon data.
  // 4. OnWebstoreParseSuccess: shows the install UI
  // 5. InstallUIProceed: initiates the .crx download/install
  //
  // All flows (whether successful or not) end up in CompleteInstall, which
  // informs our delegate of success/failure.

  // WebstoreDataFetcherDelegate interface implementation.
  void OnWebstoreRequestFailure(const std::string& extension_id) override;

  void OnWebstoreItemJSONAPIResponseParseSuccess(
      const std::string& extension_id,
      const base::Value::Dict& webstore_data) override;

  void OnFetchItemSnippetParseSuccess(
      const std::string& extension_id,
      FetchItemSnippetResponse item_snippet) override;

  void OnWebstoreResponseParseFailure(const std::string& extension_id,
                                      const std::string& error) override;

  // WebstoreInstallHelper::Delegate interface implementation.
  void OnWebstoreParseSuccess(const std::string& id,
                              const SkBitmap& icon,
                              base::Value::Dict parsed_manifest) override;
  void OnWebstoreParseFailure(const std::string& id,
                              InstallHelperResultCode result_code,
                              const std::string& error_message) override;

  // WebstoreInstaller::Delegate callbacks.
  void OnExtensionInstallSuccess(const std::string& id);
  void OnExtensionInstallFailure(const std::string& id,
                                 const std::string& error,
                                 WebstoreInstaller::FailureReason reason);

  // ProfileObserver
  void OnProfileWillBeDestroyed(Profile* profile) override;

  void ShowInstallUI();
  void OnWebStoreDataFetcherDone();

  // Called when install either completes or aborts to clean up internal
  // state and release the added reference from BeginInstall.
  void CleanUp();

  // Input configuration.
  std::string id_;
  Callback callback_;
  raw_ptr<Profile> profile_;
  base::ScopedObservation<Profile, ProfileObserver> observation_{this};
  WebstoreInstaller::InstallSource install_source_{
      WebstoreInstaller::INSTALL_SOURCE_INLINE};

  // Installation dialog and its underlying prompt.
  std::unique_ptr<ExtensionInstallPrompt> install_ui_;
  std::unique_ptr<ExtensionInstallPrompt::Prompt> install_prompt_;

  // For fetching webstore JSON data.
  std::unique_ptr<WebstoreDataFetcher> webstore_data_fetcher_;

  // Extracted from the webstore JSON data response.
  std::string localized_name_;
  std::string localized_description_;
  bool show_user_count_{true};
  std::string localized_user_count_;
  double average_rating_{0.0};
  std::string localized_rating_count_;
  int rating_count_{0};
  std::optional<base::Value::Dict> manifest_;
  SkBitmap icon_;

  // Active install registered with the InstallTracker.
  std::unique_ptr<ScopedActiveInstall> scoped_active_install_;

  // Created by ShowInstallUI() when a prompt is shown (if
  // the implementor returns a non-NULL in CreateInstallPrompt()).
  scoped_refptr<Extension> localized_extension_for_display_;

  base::WeakPtrFactory<WebstoreStandaloneInstaller> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_WEBSTORE_STANDALONE_INSTALLER_H_
