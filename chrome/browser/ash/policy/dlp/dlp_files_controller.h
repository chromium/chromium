// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_DLP_DLP_FILES_CONTROLLER_H_
#define CHROME_BROWSER_ASH_POLICY_DLP_DLP_FILES_CONTROLLER_H_

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chromeos/dbus/dlp/dlp_service.pb.h"
#include "components/file_access/scoped_file_access_copy.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "storage/browser/file_system/file_system_url.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "url/gurl.h"

namespace storage {
class FileSystemURL;
class FileSystemContext;
}  // namespace storage

namespace views {
class Widget;
}  // namespace views

namespace policy {

class DlpWarnNotifier;
class DlpFilesEventStorage;

// DlpFilesController is responsible for deciding whether file transfers are
// allowed according to the files sources saved in the DLP daemon and the rules
// of the Data leak prevention policy set by the admin.
class DlpFilesController {
 public:
  // Types of file actions. These actions are used when warning dialogs are
  // shown because of files restrictions. This is used in UMA histograms, should
  // not change order.
  enum class FileAction {
    kUnknown = 0,
    kDownload = 1,
    kTransfer = 2,
    kUpload = 3,
    kCopy = 4,
    kMove = 5,
    kOpen = 6,
    kShare = 7,
    kMaxValue = kShare
  };

  // DlpFileMetadata keeps metadata about a file, such as whether it's managed
  // or not and the source URL, if it exists.
  struct DlpFileMetadata {
    DlpFileMetadata() = delete;
    DlpFileMetadata(const std::string& source_url,
                    bool is_dlp_restricted,
                    bool is_restricted_for_destination);

    friend bool operator==(const DlpFileMetadata& a, const DlpFileMetadata& b) {
      return a.is_dlp_restricted == b.is_dlp_restricted &&
             a.source_url == b.source_url &&
             a.is_restricted_for_destination == b.is_restricted_for_destination;
    }
    friend bool operator!=(const DlpFileMetadata& a, const DlpFileMetadata& b) {
      return !(a == b);
    }

    // Source URL from which the file was downloaded.
    std::string source_url;
    // Whether the file is under any DLP rule or not.
    bool is_dlp_restricted;
    // Whether the file is restricted by DLP for a specific destination.
    bool is_restricted_for_destination;
  };

  // DlpFileRestrictionDetails keeps aggregated information about DLP rules
  // that apply to a file. It consists of the level (e.g. block, warn) and
  // destinations for which this level applies (URLs and/or components).
  struct DlpFileRestrictionDetails {
    DlpFileRestrictionDetails();
    DlpFileRestrictionDetails(const DlpFileRestrictionDetails&) = delete;
    DlpFileRestrictionDetails& operator=(const DlpFileRestrictionDetails&) =
        delete;
    DlpFileRestrictionDetails(DlpFileRestrictionDetails&&);
    DlpFileRestrictionDetails& operator=(DlpFileRestrictionDetails&&);
    ~DlpFileRestrictionDetails();

    // The level for which the restriction is enforced.
    DlpRulesManager::Level level;
    // List of URLs for which the restriction is enforced.
    std::vector<std::string> urls;
    // List of components for which the restriction is enforced.
    std::vector<DlpRulesManager::Component> components;
  };

  // FileDaemonInfo represents file info used for communication with the DLP
  // daemon.
  struct FileDaemonInfo {
    FileDaemonInfo() = delete;
    FileDaemonInfo(ino64_t inode,
                   const base::FilePath& path,
                   const std::string& source_url);

    friend bool operator==(const FileDaemonInfo& a, const FileDaemonInfo& b) {
      return a.inode == b.inode && a.path == b.path &&
             a.source_url == b.source_url;
    }
    friend bool operator!=(const FileDaemonInfo& a, const FileDaemonInfo& b) {
      return !(a == b);
    }

    // File inode.
    ino64_t inode;
    // File path.
    base::FilePath path;
    // Source URL from which the file was downloaded.
    GURL source_url;
  };

  // DlpFileDestination represents the destination for file transfer. It either
  // has a url or a component.
  struct DlpFileDestination {
    DlpFileDestination();
    explicit DlpFileDestination(const std::string& url);
    explicit DlpFileDestination(const dlp::DlpComponent component);
    explicit DlpFileDestination(const DlpRulesManager::Component component);

    DlpFileDestination(const DlpFileDestination&);
    DlpFileDestination& operator=(const DlpFileDestination&);
    DlpFileDestination(DlpFileDestination&&);
    DlpFileDestination& operator=(DlpFileDestination&&);

    bool operator==(const DlpFileDestination&) const;
    bool operator!=(const DlpFileDestination&) const;
    bool operator<(const DlpFileDestination& other) const;
    bool operator<=(const DlpFileDestination& other) const;
    bool operator>(const DlpFileDestination& other) const;
    bool operator>=(const DlpFileDestination& other) const;

    ~DlpFileDestination();

    // Destination url or destination path.
    absl::optional<std::string> url_or_path;
    // Destination component.
    absl::optional<DlpRulesManager::Component> component;
  };

  using GetDisallowedTransfersCallback =
      base::OnceCallback<void(std::vector<storage::FileSystemURL>)>;
  using GetFilesRestrictedByAnyRuleCallback = GetDisallowedTransfersCallback;
  using FilterDisallowedUploadsCallback =
      base::OnceCallback<void(std::vector<ui::SelectedFileInfo>)>;
  using CheckIfDownloadAllowedCallback = base::OnceCallback<void(bool)>;
  using CheckIfLaunchAllowedCallback = base::OnceCallback<void(bool)>;
  using GetDlpMetadataCallback =
      base::OnceCallback<void(std::vector<DlpFileMetadata>)>;
  using IsFilesTransferRestrictedCallback =
      base::OnceCallback<void(const std::vector<FileDaemonInfo>&)>;

  explicit DlpFilesController(const DlpRulesManager& rules_manager);
  DlpFilesController(const DlpFilesController& other) = delete;
  DlpFilesController& operator=(const DlpFilesController& other) = delete;

  virtual ~DlpFilesController();

  // Returns a sublist of |transferred_files| disallowed to be transferred to
  // |destination| in |result_callback|. |is_move| is true if it's a move
  // operation. Otherwise it's false.
  void GetDisallowedTransfers(
      const std::vector<storage::FileSystemURL>& transferred_files,
      storage::FileSystemURL destination,
      bool is_move,
      GetDisallowedTransfersCallback result_callback);

  // Retrieves metadata for each entry in |files| and returns it as a list in
  // |result_callback|. If |destination| is passed, marks the files that are not
  // allowed to be uploaded to that particular destination.
  void GetDlpMetadata(const std::vector<storage::FileSystemURL>& files,
                      absl::optional<DlpFileDestination> destination,
                      GetDlpMetadataCallback result_callback);

  // Filters files disallowed to be uploaded to `destination`.
  virtual void FilterDisallowedUploads(
      std::vector<ui::SelectedFileInfo> selected_files,
      const DlpFileDestination& destination,
      FilterDisallowedUploadsCallback result_callback);

  // Checks whether the file download from `download_src` to `file_path` is
  // allowed.
  virtual void CheckIfDownloadAllowed(
      const DlpFileDestination& download_src,
      const base::FilePath& file_path,
      CheckIfDownloadAllowedCallback result_callback);

  // Returns whether downloads from `download_src` to `file_path` might be
  // blocked by DLP, and so a picker should be shown.
  virtual bool ShouldPromptBeforeDownload(
      const DlpFileDestination& download_src,
      const base::FilePath& file_path);

  // Checks whether launching `app_update` with `intent` is allowed.
  void CheckIfLaunchAllowed(const apps::AppUpdate& app_update,
                            apps::IntentPtr intent,
                            CheckIfLaunchAllowedCallback result_callback);

  // Returns true if `app_update` is blocked from opening any of the
  // files in `intent`.
  virtual bool IsLaunchBlocked(const apps::AppUpdate& app_update,
                               const apps::IntentPtr& intent);

  // Returns a sublist of |transferred_files| which aren't allowed to be
  // transferred to either |destination_url| or |destination_component| in
  // |result_callback|.
  void IsFilesTransferRestricted(
      const std::vector<FileDaemonInfo>& transferred_files,
      const DlpFileDestination& destination,
      FileAction files_action,
      IsFilesTransferRestrictedCallback result_callback);

  // Returns restriction information for `source_url`.
  std::vector<DlpFileRestrictionDetails> GetDlpRestrictionDetails(
      const std::string& source_url);

  // Returns a list of components to which the transfer of a file with
  // `source_url` is blocked.
  std::vector<DlpRulesManager::Component> GetBlockedComponents(
      const std::string& source_url);

  // Returns whether a dlp policy matches for the `file`.
  bool IsDlpPolicyMatched(const FileDaemonInfo& file);

  // Requests ScopedFileAccess for |source| for the operation to copy from
  // |source| to |destination|.
  virtual void RequestCopyAccess(
      const storage::FileSystemURL& source,
      const storage::FileSystemURL& destination,
      base::OnceCallback<void(std::unique_ptr<file_access::ScopedFileAccess>)>
          result_callback);

  void SetWarnNotifierForTesting(
      std::unique_ptr<DlpWarnNotifier> warn_notifier);

  DlpFilesEventStorage* GetEventStorageForTesting();

  void SetFileSystemContextForTesting(
      storage::FileSystemContext* file_system_context);

 private:
  // Called back from warning dialog. Passes blocked files sources along
  // to |callback|. In case |should_proceed| is true, passes only
  // |restricted_files_sources|, otherwise passes also |warned_files_sources|.
  void OnDlpWarnDialogReply(
      std::vector<FileDaemonInfo> restricted_files_sources,
      std::vector<FileDaemonInfo> warned_files_sources,
      std::vector<std::string> warned_src_patterns,
      const DlpFileDestination& dst,
      const absl::optional<std::string>& dst_pattern,
      FileAction files_action,
      IsFilesTransferRestrictedCallback callback,
      bool should_proceed);

  void ReturnDisallowedTransfers(
      base::flat_map<std::string, storage::FileSystemURL> files_map,
      GetDisallowedTransfersCallback result_callback,
      dlp::CheckFilesTransferResponse response);

  void ReturnAllowedUploads(std::vector<ui::SelectedFileInfo> uploaded_files,
                            FilterDisallowedUploadsCallback result_callback,
                            dlp::CheckFilesTransferResponse response);

  void ReturnDlpMetadata(std::vector<absl::optional<ino64_t>> inodes,
                         absl::optional<DlpFileDestination> destination,
                         GetDlpMetadataCallback result_callback,
                         const ::dlp::GetFilesSourcesResponse response);

  void LaunchIfAllowed(CheckIfLaunchAllowedCallback result_callback,
                       ::dlp::CheckFilesTransferResponse response);

  // Reports an event if a `DlpReportingManager` instance exists. When
  // `dst_pattern` is missing, we report `dst.component.value()` instead. When
  // `level` is missing, we report a warning proceeded event.
  void MaybeReportEvent(ino64_t inode,
                        const base::FilePath& path,
                        const std::string& source_pattern,
                        const DlpFileDestination& dst,
                        const absl::optional<std::string>& dst_pattern,
                        absl::optional<DlpRulesManager::Level> level);

  // Closes warning dialog if `response` has error.
  void MaybeCloseDialog(::dlp::CheckFilesTransferResponse response);

  // Called when `transferred_files` is ready. Constructs CheckFilesTransfer
  // request and forwards it to the dlp daemon.
  void ContinueGetDisallowedTransfers(
      storage::FileSystemURL destination,
      bool is_move,
      GetDisallowedTransfersCallback result_callback,
      std::vector<storage::FileSystemURL> transferred_files);

  // Called when `uploaded_files` is ready. Constructs CheckFilesTransfer
  // request and forwards it to the dlp daemon.
  void ContinueFilterDisallowedUploads(
      std::vector<ui::SelectedFileInfo> selected_files,
      const DlpFileDestination& destination,
      FilterDisallowedUploadsCallback result_callback,
      std::vector<storage::FileSystemURL> uploaded_files);

  const DlpRulesManager& rules_manager_;

  // Is used for creating and showing the warning dialog.
  std::unique_ptr<DlpWarnNotifier> warn_notifier_;
  // Pointer to the associated DlpWarnDialog widget.
  // Not null only while the dialog is opened.
  base::WeakPtr<views::Widget> warn_dialog_widget_ = nullptr;

  // Keeps track of events and detects duplicate ones using time based
  // approach.
  std::unique_ptr<DlpFilesEventStorage> event_storage_;

  base::WeakPtrFactory<DlpFilesController> weak_ptr_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_DLP_DLP_FILES_CONTROLLER_H_
