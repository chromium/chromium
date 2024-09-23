// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_CHROME_DOWNLOAD_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_DOWNLOAD_CHROME_DOWNLOAD_MANAGER_DELEGATE_H_

#include <stdint.h>

#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "chrome/browser/download/download_completion_blocker.h"
#include "chrome/browser/download/download_target_determiner_delegate.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_path_reservation_tracker.h"
#include "components/download/public/common/download_target_info.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/content/common/proto/download_file_types.pb.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_manager_delegate.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/shell_dialogs/selected_file_info.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/download/android/download_dialog_bridge.h"
#include "chrome/browser/download/android/download_message_bridge.h"
#endif

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#endif

class DownloadPrefs;
class Profile;

namespace content {
class DownloadManager;
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
namespace extensions {
class CrxInstaller;
class CrxInstallError;
}
#endif

// This is the Chrome side helper for the download system.
class ChromeDownloadManagerDelegate
    : public content::DownloadManagerDelegate,
      public DownloadTargetDeterminerDelegate,
      public content::DownloadManager::Observer {
 public:
  explicit ChromeDownloadManagerDelegate(Profile* profile);

  ChromeDownloadManagerDelegate(const ChromeDownloadManagerDelegate&) = delete;
  ChromeDownloadManagerDelegate& operator=(
      const ChromeDownloadManagerDelegate&) = delete;

  ~ChromeDownloadManagerDelegate() override;

  // Should be called before the first call to ShouldCompleteDownload() to
  // disable SafeBrowsing checks for |item|.
  static void DisableSafeBrowsing(download::DownloadItem* item);

  // True when |danger_type| is one that is blocked for policy reasons (e.g.
  // "file too large") as opposed to malicious content reasons.
  static bool IsDangerTypeBlocked(download::DownloadDangerType danger_type);

  void SetDownloadManager(content::DownloadManager* dm);

#if BUILDFLAG(IS_ANDROID)
  void ShowDownloadDialog(gfx::NativeWindow native_window,
                          int64_t total_bytes,
                          DownloadLocationDialogType dialog_type,
                          const base::FilePath& suggested_path,
                          DownloadDialogBridge::DialogCallback callback);

  void SetDownloadDialogBridgeForTesting(DownloadDialogBridge* bridge);
  void SetDownloadMessageBridgeForTesting(DownloadMessageBridge* bridge);
#endif

  // Callbacks passed to GetNextId() will not be called until the returned
  // callback is called.
  content::DownloadIdCallback GetDownloadIdReceiverCallback();

  // content::DownloadManagerDelegate
  void Shutdown() override;
  void OnDownloadCanceledAtShutdown(download::DownloadItem* item) override;
  void GetNextId(content::DownloadIdCallback callback) override;
  bool DetermineDownloadTarget(
      download::DownloadItem* item,
      download::DownloadTargetCallback* callback) override;
  bool ShouldAutomaticallyOpenFile(const GURL& url,
                                   const base::FilePath& path) override;
  bool ShouldAutomaticallyOpenFileByPolicy(const GURL& url,
                                           const base::FilePath& path) override;
  bool ShouldCompleteDownload(download::DownloadItem* item,
                              base::OnceClosure complete_callback) override;
  bool ShouldOpenDownload(
      download::DownloadItem* item,
      content::DownloadOpenDelayedCallback callback) override;
  bool ShouldObfuscateDownload(download::DownloadItem* item) override;
  bool InterceptDownloadIfApplicable(
      const GURL& url,
      const std::string& user_agent,
      const std::string& content_disposition,
      const std::string& mime_type,
      const std::string& request_origin,
      int64_t content_length,
      bool is_transient,
      content::WebContents* web_contents) override;
  void GetSaveDir(content::BrowserContext* browser_context,
                  base::FilePath* website_save_dir,
                  base::FilePath* download_save_dir) override;
  void ChooseSavePath(content::WebContents* web_contents,
                      const base::FilePath& suggested_path,
                      const base::FilePath::StringType& default_extension,
                      bool can_save_as_complete,
                      content::SavePackagePathPickedCallback callback) override;
  void SanitizeSavePackageResourceName(base::FilePath* filename,
                                       const GURL& source_url) override;
  void SanitizeDownloadParameters(
      download::DownloadUrlParameters* params) override;
  void OpenDownload(download::DownloadItem* download) override;
  void ShowDownloadInShell(download::DownloadItem* download) override;
  std::string ApplicationClientIdForFileScanning() override;
  void CheckDownloadAllowed(
      const content::WebContents::Getter& web_contents_getter,
      const GURL& url,
      const std::string& request_method,
      std::optional<url::Origin> request_initiator,
      bool from_download_cross_origin_redirect,
      bool content_initiated,
      const std::string& mime_type,
      std::optional<ui::PageTransition> page_transition,
      content::CheckDownloadAllowedCallback check_download_allowed_cb) override;
  download::QuarantineConnectionCallback GetQuarantineConnectionCallback()
      override;
  std::unique_ptr<download::DownloadItemRenameHandler>
  GetRenameHandlerForDownload(download::DownloadItem* download_item) override;
  void CheckSavePackageAllowed(
      download::DownloadItem* download_item,
      base::flat_map<base::FilePath, base::FilePath> save_package_files,
      content::SavePackageAllowedCallback callback) override;
#if BUILDFLAG(IS_ANDROID)
  bool IsFromExternalApp(download::DownloadItem* item) override;
  bool ShouldOpenPdfInline() override;
  bool IsDownloadRestrictedByPolicy() override;
#else
  void AttachExtraInfo(download::DownloadItem* item) override;
#endif  // BUILDFLAG(IS_ANDROID)

  // Opens a download using the platform handler. DownloadItem::OpenDownload,
  // which ends up being handled by OpenDownload(), will open a download in the
  // browser if doing so is preferred.
  void OpenDownloadUsingPlatformHandler(download::DownloadItem* download);

  DownloadPrefs* download_prefs() { return download_prefs_.get(); }

#if BUILDFLAG(FULL_SAFE_BROWSING)
  // The state of a safebrowsing check.
  class SafeBrowsingState : public DownloadCompletionBlocker {
   public:
    SafeBrowsingState() = default;

    SafeBrowsingState(const SafeBrowsingState&) = delete;
    SafeBrowsingState& operator=(const SafeBrowsingState&) = delete;

    ~SafeBrowsingState() override;

    // String pointer used for identifying safebrowsing data associated with
    // a download item.
    static const char kSafeBrowsingUserDataKey[];
  };

  // Callback function after the DownloadProtectionService completes.
  void CheckClientDownloadDone(uint32_t download_id,
                               safe_browsing::DownloadCheckResult result);

  // Callback function after scanning completes for a save package.
  void CheckSavePackageScanningDone(uint32_t download_id,
                                    safe_browsing::DownloadCheckResult result);
#endif  // FULL_SAFE_BROWSING

  base::WeakPtr<ChromeDownloadManagerDelegate> GetWeakPtr();

  static void ConnectToQuarantineService(
      mojo::PendingReceiver<quarantine::mojom::Quarantine> receiver);

  // Return true if the downloaded file should be blocked based on the current
  // download restriction pref, the file type, and |danger_type|.
  bool ShouldBlockFile(download::DownloadItem* item,
                       download::DownloadDangerType danger_type) const;

#if !BUILDFLAG(IS_ANDROID)
  // Schedules the ephemeral warning download to be canceled. It will only be
  // canceled if it continues to be an ephemeral warning that hasn't been acted
  // on when the scheduled time arrives.
  void ScheduleCancelForEphemeralWarning(const std::string& guid);
#endif

  // Returns true if |path| should open in the browser.
  virtual bool IsOpenInBrowserPreferredForFile(const base::FilePath& path);

 protected:
#if BUILDFLAG(FULL_SAFE_BROWSING)
  virtual safe_browsing::DownloadProtectionService*
      GetDownloadProtectionService();
#endif

  // Show file picker for |download|.
  virtual void ShowFilePickerForDownload(
      download::DownloadItem* download,
      const base::FilePath& suggested_path,
      DownloadTargetDeterminerDelegate::ConfirmationCallback callback);

  // DownloadTargetDeterminerDelegate. Protected for testing.
  void GetInsecureDownloadStatus(
      download::DownloadItem* download,
      const base::FilePath& virtual_path,
      GetInsecureDownloadStatusCallback callback) override;
  void NotifyExtensions(download::DownloadItem* download,
                        const base::FilePath& suggested_virtual_path,
                        NotifyExtensionsCallback callback) override;
  void ReserveVirtualPath(
      download::DownloadItem* download,
      const base::FilePath& virtual_path,
      bool create_directory,
      download::DownloadPathReservationTracker::FilenameConflictAction
          conflict_action,
      ReservedPathCallback callback) override;
#if BUILDFLAG(IS_ANDROID)
  void RequestIncognitoWarningConfirmation(
      IncognitoWarningConfirmationCallback) override;
#endif
  void RequestConfirmation(download::DownloadItem* download,
                           const base::FilePath& suggested_virtual_path,
                           DownloadConfirmationReason reason,
                           ConfirmationCallback callback) override;
  void DetermineLocalPath(download::DownloadItem* download,
                          const base::FilePath& virtual_path,
                          download::LocalPathCallback callback) override;
  void CheckDownloadUrl(download::DownloadItem* download,
                        const base::FilePath& suggested_virtual_path,
                        CheckDownloadUrlCallback callback) override;
  void GetFileMimeType(const base::FilePath& path,
                       GetFileMimeTypeCallback callback) override;

#if BUILDFLAG(IS_ANDROID)
  virtual void OnDownloadCanceled(download::DownloadItem* download,
                                  bool has_no_external_storage);
#endif

  // Called when the file picker returns the confirmation result.
  void OnConfirmationCallbackComplete(
      DownloadTargetDeterminerDelegate::ConfirmationCallback callback,
      DownloadConfirmationResult result,
      const ui::SelectedFileInfo& selected_file_info);

  // So that test classes that inherit from this for override purposes
  // can call back into the DownloadManager.
  raw_ptr<content::DownloadManager> download_manager_ = nullptr;

 private:
  friend class base::RefCountedThreadSafe<ChromeDownloadManagerDelegate>;
  FRIEND_TEST_ALL_PREFIXES(ChromeDownloadManagerDelegateTest,
                           RequestConfirmation_Android);
  FRIEND_TEST_ALL_PREFIXES(ChromeDownloadManagerDelegateTest,
                           CancelAllEphemeralWarnings);

  using IdCallbackVector = std::vector<content::DownloadIdCallback>;

  // Called to show a file picker for download with |guid|
  void ShowFilePicker(
      const std::string& guid,
      const base::FilePath& suggested_path,
      DownloadTargetDeterminerDelegate::ConfirmationCallback callback);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Called when CrxInstaller in running_crx_installs_ finishes installation.
  void OnInstallerDone(const base::UnguessableToken& token,
                       content::DownloadOpenDelayedCallback callback,
                       const std::optional<extensions::CrxInstallError>& error);
#endif

  // Internal gateways for ShouldCompleteDownload().
  bool IsDownloadReadyForCompletion(
      download::DownloadItem* item,
      base::OnceClosure internal_complete_callback);
  void ShouldCompleteDownloadInternal(uint32_t download_id,
                                      base::OnceClosure user_complete_callback);

  // Sets the next download id based on download database records, and runs all
  // cached id callbacks.
  void SetNextId(uint32_t id);

  // Runs the |callback| with next id. Results in the download being started.
  void ReturnNextId(content::DownloadIdCallback callback);

  void OnDownloadTargetDetermined(
      uint32_t download_id,
      download::DownloadTargetCallback callback,
      download::DownloadTargetInfo target_info,
      safe_browsing::DownloadFileType::DangerLevel danger_level);

  // Sends a download report when the dangerous download is opened. This action
  // can be performed multiple times after the warning is bypassed, so this
  // report can be sent multiple times for a single download.
  void MaybeSendDangerousDownloadOpenedReport(download::DownloadItem* download,
                                              bool show_download_in_folder);

  // Sends a download report when the dangerous download is canceled
  // automatically. This code path is NOT triggered if a user explicitly
  // canceled the download.
  void MaybeSendDangerousDownloadCanceledReport(
      download::DownloadItem* download,
      bool is_shutdown);

  void OnCheckDownloadAllowedComplete(
      content::CheckDownloadAllowedCallback check_download_allowed_cb,
      bool storage_permission_granted,
      bool allow);

  // Returns whether this is the most recent download in the rare event where
  // multiple downloads are associated with the same file path.
  bool IsMostRecentDownloadItemAtFilePath(download::DownloadItem* download);

#if !BUILDFLAG(IS_ANDROID)
  // Cancels a download if it's still an ephemeral warning (and has not been
  // acted on by the user).
  void CancelForEphemeralWarning(const std::string& guid);
  // If the browser doesn't shut down cleanly, there can be ephemeral warnings
  // that were not cleaned up. This function cleans them up on startup, when the
  // download manager is initialized.
  void CancelAllEphemeralWarnings();
#endif

  // content::DownloadManager::Observer
  void OnManagerInitialized() override;

#if BUILDFLAG(IS_ANDROID)
  // Called after a unique file name is generated in the case that there is a
  // TARGET_CONFLICT and the new file name should be displayed to the user.
  void GenerateUniqueFileNameDone(
      const std::string& download_guid,
      DownloadTargetDeterminerDelegate::ConfirmationCallback callback,
      download::PathValidationResult result,
      const base::FilePath& target_path);
#endif

  raw_ptr<Profile, DanglingUntriaged> profile_;

#if BUILDFLAG(IS_ANDROID)
  std::unique_ptr<DownloadDialogBridge> download_dialog_bridge_;
  std::unique_ptr<DownloadMessageBridge> download_message_bridge_;
#endif

  // If history database fails to initialize, this will always be kInvalidId.
  // Otherwise, the first available download id is assigned from history
  // database, and incremented by one for each download.
  uint32_t next_download_id_;

  // Whether |next_download_id_| is retrieved from history db.
  bool next_id_retrieved_;

  // The |GetNextId| callbacks that may be cached before loading the download
  // database.
  IdCallbackVector id_callbacks_;
  std::unique_ptr<DownloadPrefs> download_prefs_;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // CRX installs that are currently in progress.
  std::map<base::UnguessableToken, scoped_refptr<extensions::CrxInstaller>>
      running_crx_installs_;
#endif

  // Outstanding callbacks to open file selection dialog.
  std::deque<base::OnceClosure> file_picker_callbacks_;

  // Whether a file picker dialog is showing.
  bool is_file_picker_showing_;

  base::WeakPtrFactory<ChromeDownloadManagerDelegate> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_DOWNLOAD_CHROME_DOWNLOAD_MANAGER_DELEGATE_H_
