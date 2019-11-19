// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_WEBSTORE_INSTALLER_H_
#define CHROME_BROWSER_EXTENSIONS_WEBSTORE_INSTALLER_H_

#include <list>
#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/scoped_observer.h"
#include "base/supports_user_data.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/download_item.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/manifest_handlers/shared_module_info.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

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
class Manifest;

// Downloads and installs extensions from the web store.
class WebstoreInstaller : public content::NotificationObserver,
                          public ExtensionRegistryObserver,
                          public download::DownloadItem::Observer,
                          public content::WebContentsObserver,
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

  enum ManifestCheckLevel {
    // Do not check for any manifest equality.
    MANIFEST_CHECK_LEVEL_NONE,

    // Only check that the expected and actual permissions have the same
    // effective permissions.
    MANIFEST_CHECK_LEVEL_LOOSE,

    // All data in the expected and actual manifests must match.
    MANIFEST_CHECK_LEVEL_STRICT,
  };

  class Delegate {
   public:
    virtual void OnExtensionDownloadStarted(const std::string& id,
                                            download::DownloadItem* item);
    virtual void OnExtensionDownloadProgress(const std::string& id,
                                             download::DownloadItem* item);
    virtual void OnExtensionInstallSuccess(const std::string& id) = 0;
    virtual void OnExtensionInstallFailure(const std::string& id,
                                           const std::string& error,
                                           FailureReason reason) = 0;

   protected:
    virtual ~Delegate() {}
  };

  // Contains information about what parts of the extension install process can
  // be skipped or modified. If one of these is present, it means that a CRX
  // download was initiated by WebstoreInstaller. The Approval instance should
  // be checked further for additional details.
  struct Approval : public base::SupportsUserData::Data {
    static std::unique_ptr<Approval> CreateWithInstallPrompt(Profile* profile);

    // Creates an Approval for installing a shared module.
    static std::unique_ptr<Approval> CreateForSharedModule(Profile* profile);

    // Creates an Approval that will skip putting up an install confirmation
    // prompt if the actual manifest from the extension to be installed matches
    // |parsed_manifest|. The |strict_manifest_check| controls whether we want
    // to require an exact manifest match, or are willing to tolerate a looser
    // check just that the effective permissions are the same.
    static std::unique_ptr<Approval> CreateWithNoInstallPrompt(
        Profile* profile,
        const std::string& extension_id,
        std::unique_ptr<base::DictionaryValue> parsed_manifest,
        bool strict_manifest_check);

    ~Approval() override;

    // The extension id that was approved for installation.
    std::string extension_id;

    // The profile the extension should be installed into.
    Profile* profile;

    // The expected manifest, before localization.
    std::unique_ptr<Manifest> manifest;

    // Whether to use a bubble notification when an app is installed, instead of
    // the default behavior of transitioning to the new tab page.
    bool use_app_installed_bubble;

    // Whether to skip the post install UI like the extension installed bubble.
    bool skip_post_install_ui;

    // Whether to skip the install dialog once the extension has been downloaded
    // and unpacked. One reason this can be true is that in the normal webstore
    // installation, the dialog is shown earlier, before any download is done,
    // so there's no need to show it again.
    bool skip_install_dialog;

    // Manifest check level for checking actual manifest against expected
    // manifest.
    ManifestCheckLevel manifest_check_level;

    // Used to show the install dialog.
    ExtensionInstallPrompt::ShowDialogCallback show_dialog_callback;

    // The icon to use to display the extension while it is installing.
    gfx::ImageSkia installing_icon;

    // A dummy extension created from |manifest|;
    scoped_refptr<Extension> dummy_extension;

    // Required minimum version.
    std::unique_ptr<base::Version> minimum_version;

    // The authuser index required to download the item being installed. May be
    // the empty string, in which case no authuser parameter is used.
    std::string authuser;

   private:
    Approval();
  };

  // Gets the Approval associated with the |download|, or NULL if there's none.
  // Note that the Approval is owned by |download|.
  static const Approval* GetAssociatedApproval(
      const download::DownloadItem& download);

  // Creates a WebstoreInstaller for downloading and installing the extension
  // with the given |id| from the Chrome Web Store. If |delegate| is not NULL,
  // it will be notified when the install succeeds or fails. The installer will
  // use the specified |controller| to download the extension. Only one
  // WebstoreInstaller can use a specific controller at any given time. This
  // also associates the |approval| with this install.
  // Note: the delegate should stay alive until being called back.
  WebstoreInstaller(Profile* profile,
                    Delegate* delegate,
                    content::WebContents* web_contents,
                    const std::string& id,
                    std::unique_ptr<Approval> approval,
                    InstallSource source);

  // Starts downloading and installing the extension.
  void Start();

  // content::NotificationObserver.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // ExtensionRegistryObserver.
  void OnExtensionInstalled(content::BrowserContext* browser_context,
                            const Extension* extension,
                            bool is_update) override;

  // Removes the reference to the delegate passed in the constructor. Used when
  // the delegate object must be deleted before this object.
  void InvalidateDelegate();

  // Instead of using the default download directory, use |directory| instead.
  // This does *not* transfer ownership of |directory|.
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

  // Downloads next pending module in |pending_modules_|.
  void DownloadNextPendingModule();

  // Downloads and installs a single Crx with the given |extension_id|.
  // This function is used for both the extension Crx and dependences.
  void DownloadCrx(const std::string& extension_id, InstallSource source);

  // Starts downloading the extension with ID |extension_id| to |file_path|.
  void StartDownload(const std::string& extension_id,
                     const base::FilePath& file_path);

  // Updates the InstallTracker with the latest download progress.
  void UpdateDownloadProgress();

  // Creates and starts CrxInstaller for the downloaded extension package.
  void StartCrxInstaller(const download::DownloadItem& item);

  // Reports an install |error| to the delegate for the given extension if this
  // managed its installation. This also removes the associated PendingInstall.
  void ReportFailure(const std::string& error, FailureReason reason);

  // Reports a successful install to the delegate for the given extension if
  // this managed its installation. This also removes the associated
  // PendingInstall.
  void ReportSuccess();

  // Records stats regarding an interrupted webstore download item.
  void RecordInterrupt(const download::DownloadItem* download) const;

  content::NotificationRegistrar registrar_;
  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_{this};
  Profile* profile_;
  Delegate* delegate_;
  std::string id_;
  InstallSource install_source_;
  // The DownloadItem is owned by the DownloadManager and is valid from when
  // OnDownloadStarted is called (with no error) until OnDownloadDestroyed().
  download::DownloadItem* download_item_;
  // Used to periodically update the extension's download status. This will
  // trigger at least every second, though sometimes more frequently (depending
  // on number of modules, etc).
  base::OneShotTimer download_progress_timer_;
  std::unique_ptr<Approval> approval_;
  GURL download_url_;
  scoped_refptr<CrxInstaller> crx_installer_;

  // Pending modules.
  std::list<SharedModuleInfo::ImportInfo> pending_modules_;
  // Total extension modules we need download and install (the main module and
  // depedences).
  int total_modules_;
  bool download_started_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_WEBSTORE_INSTALLER_H_
