// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_WEBSTORE_INSTALLER_H_
#define CHROME_BROWSER_EXTENSIONS_WEBSTORE_INSTALLER_H_

#include <list>
#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "base/version.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/download_item.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/install/crx_install_error.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/manifest_handlers/shared_module_info.h"
#include "url/gurl.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

class Profile;

namespace base {
class FilePath;
}

namespace content {
class WebContents;
}

namespace extensions {

class CrxInstaller;
class Extension;
class ExtensionRegistry;
struct InstallApproval;

// Downloads and installs extensions from the web store.
class WebstoreInstaller : public ExtensionRegistryObserver,
                          public download::DownloadItem::Observer,
                          public base::RefCountedThreadSafe<
                              WebstoreInstaller,
                              content::BrowserThread::DeleteOnUIThread> {
 public:
  enum InstallSource {
    // Inline installs trigger slightly different behavior (install source
    // is different, download referrers are the item's page in the gallery).
    // TODO(ackermanb): Remove once server side metrics (omaha) tracking with
    // this enum is figured out with any of the subclasses of
    // WebstoreStandaloneInstaller.
    INSTALL_SOURCE_INLINE,
    INSTALL_SOURCE_APP_LAUNCHER,
    INSTALL_SOURCE_OTHER
  };

  enum FailureReason {
    FAILURE_REASON_CANCELLED,
    FAILURE_REASON_DEPENDENCY_NOT_FOUND,
    FAILURE_REASON_DEPENDENCY_NOT_SHARED_MODULE,
    FAILURE_REASON_OTHER
  };

  using SuccessCallback = base::OnceCallback<void(const std::string&)>;
  using FailureCallback = base::OnceCallback<
      void(const std::string&, const std::string&, FailureReason)>;

  // Gets the InstallApproval associated with the `download`, or nullptr if
  // there's none. Note that the InstallApproval is owned by `download`.
  static const InstallApproval* GetAssociatedApproval(
      const download::DownloadItem& download);

  // Creates a WebstoreInstaller for downloading and installing the extension
  // with the given `id` from the Chrome Web Store. The `success_callback` and
  // `failure_callback` parameters must not be null. This also associates the
  // `approval` with this install.
  WebstoreInstaller(Profile* profile,
                    SuccessCallback success_callback,
                    FailureCallback failure_callback,
                    content::WebContents* web_contents,
                    const std::string& id,
                    std::unique_ptr<InstallApproval> approval,
                    InstallSource source);

  // Starts downloading and installing the extension.
  void Start();

  // ExtensionRegistryObserver.
  void OnExtensionInstalled(content::BrowserContext* browser_context,
                            const Extension* extension,
                            bool is_update) override;

  // Instead of using the default download directory, use `directory` instead.
  // This does *not* transfer ownership of `directory`.
  static void SetDownloadDirectoryForTests(base::FilePath* directory);

 protected:
  // For testing.
  ~WebstoreInstaller() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(WebstoreInstallerTest, PlatformParams);
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<WebstoreInstaller>;

  // Helper to get install URL.
  static GURL GetWebstoreInstallURL(const std::string& extension_id,
                                    InstallSource source);

  // DownloadManager::DownloadUrl callback.
  void OnDownloadStarted(const std::string& extension_id,
                         download::DownloadItem* item,
                         download::DownloadInterruptReason interrupt_reason);

  // DownloadItem::Observer implementation:
  void OnDownloadUpdated(download::DownloadItem* download) override;
  void OnDownloadDestroyed(download::DownloadItem* download) override;

  // Downloads next pending module in `pending_modules_`.
  void DownloadNextPendingModule();

  // Downloads and installs a single Crx with the given `extension_id`.
  // This function is used for both the extension Crx and dependences.
  void DownloadCrx(const std::string& extension_id, InstallSource source);

  // Starts downloading the extension with ID `extension_id` to `file_path`.
  void StartDownload(const std::string& extension_id,
                     const base::FilePath& file_path);

  // Updates the InstallTracker with the latest download progress.
  void UpdateDownloadProgress();

  // Creates and starts CrxInstaller for the downloaded extension package.
  void StartCrxInstaller(const download::DownloadItem& item);

  // Reports an install `error` to the delegate for the given extension if this
  // managed its installation. This also removes the associated PendingInstall.
  void ReportFailure(const std::string& error, FailureReason reason);

  // Reports a successful install to the delegate for the given extension if
  // this managed its installation. This also removes the associated
  // PendingInstall.
  void ReportSuccess();

  // Called when crx_installer_->InstallCrx() finishes.
  void OnInstallerDone(const std::optional<CrxInstallError>& error);

  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observation_{this};
  base::WeakPtr<content::WebContents> web_contents_;
  raw_ptr<Profile> profile_;
  SuccessCallback success_callback_;
  FailureCallback failure_callback_;
  std::string id_;
  InstallSource install_source_;
  // The DownloadItem is owned by the DownloadManager and is valid from when
  // OnDownloadStarted is called (with no error) until OnDownloadDestroyed().
  raw_ptr<download::DownloadItem, DanglingUntriaged> download_item_ = nullptr;
  // Used to periodically update the extension's download status. This will
  // trigger at least every second, though sometimes more frequently (depending
  // on number of modules, etc).
  base::OneShotTimer download_progress_timer_;
  std::unique_ptr<InstallApproval> approval_;
  GURL download_url_;
  scoped_refptr<CrxInstaller> crx_installer_;

  // Pending modules.
  std::list<SharedModuleInfo::ImportInfo> pending_modules_;
  // Total extension modules we need download and install (the main module and
  // depedences).
  int total_modules_ = 0;
  bool download_started_ = false;

  base::WeakPtrFactory<WebstoreInstaller> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_WEBSTORE_INSTALLER_H_
