// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_DLP_DLP_FILES_CONTROLLER_ASH_H_
#define CHROME_BROWSER_ASH_POLICY_DLP_DLP_FILES_CONTROLLER_ASH_H_

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/file_manager/volume_manager_observer.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_file_destination.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_controller.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_utils.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chromeos/dbus/dlp/dlp_service.pb.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "storage/browser/file_system/file_system_url.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/shell_dialogs/selected_file_info.h"

namespace storage {
class FileSystemURL;
}  // namespace storage

namespace views {
class Widget;
}  // namespace views

namespace policy {

class DlpFilesEventStorage;
class DlpExtractIOTaskObserver;

// DlpFilesControllerAsh is responsible for deciding whether file transfers are
// allowed according to the files sources saved in the DLP daemon and the rules
// of the Data leak prevention policy set by the admin.
class DlpFilesControllerAsh : public DlpFilesController,
                              public file_manager::VolumeManagerObserver {
 public:
  // Returns the instance if it exists.
  static DlpFilesControllerAsh* GetForPrimaryProfile();

  // DlpFileMetadata keeps metadata about a file, such as whether it's managed
  // or not and the source URL, if it exists.
  struct DlpFileMetadata {
    DlpFileMetadata() = delete;
    DlpFileMetadata(const std::string& source_url,
                    const std::string& referrer_url,
                    bool is_dlp_restricted,
                    bool is_restricted_for_destination);

    friend bool operator==(const DlpFileMetadata& a, const DlpFileMetadata& b) {
      return a.is_dlp_restricted == b.is_dlp_restricted &&
             a.is_restricted_for_destination ==
                 b.is_restricted_for_destination &&
             a.source_url == b.source_url && a.referrer_url == b.referrer_url;
    }
    friend bool operator!=(const DlpFileMetadata& a, const DlpFileMetadata& b) {
      return !(a == b);
    }

    // Source URL from which the file was downloaded.
    std::string source_url;
    // Referrer URL from which the download process was initiated.
    std::string referrer_url;
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
    std::vector<data_controls::Component> components;
  };

  using CheckIfTransferAllowedCallback =
      base::OnceCallback<void(std::vector<storage::FileSystemURL>)>;
  using GetFilesRestrictedByAnyRuleCallback = CheckIfTransferAllowedCallback;
  using FilterDisallowedUploadsCallback =
      base::OnceCallback<void(std::vector<ui::SelectedFileInfo>)>;
  using GetDlpMetadataCallback =
      base::OnceCallback<void(std::vector<DlpFileMetadata>)>;
  using IsFilesTransferRestrictedCallback = base::OnceCallback<void(
      const std::vector<std::pair<FileDaemonInfo, ::dlp::RestrictionLevel>>&)>;

  DlpFilesControllerAsh(const DlpRulesManager& rules_manager, Profile* profile);
  DlpFilesControllerAsh(const DlpFilesControllerAsh& other) = delete;
  DlpFilesControllerAsh& operator=(const DlpFilesControllerAsh& other) = delete;

  ~DlpFilesControllerAsh() override;

  // Returns a sublist of |transferred_files| disallowed to be transferred to
  // |destination| in |result_callback|. |is_move| is true if it's a move
  // operation. Otherwise it's false.
  virtual void CheckIfTransferAllowed(
      std::optional<file_manager::io_task::IOTaskId> task_id,
      const std::vector<storage::FileSystemURL>& transferred_files,
      storage::FileSystemURL destination,
      bool is_move,
      CheckIfTransferAllowedCallback result_callback);

  // Retrieves metadata for each entry in |files| and returns it as a list in
  // |result_callback|. If |destination| is passed, marks the files that are not
  // allowed to be uploaded to that particular destination.
  virtual void GetDlpMetadata(const std::vector<storage::FileSystemURL>& files,
                              std::optional<DlpFileDestination> destination,
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
      CheckIfDlpAllowedCallback result_callback);

  // Returns whether downloads from `download_src` to `file_path` might be
  // blocked by DLP, and so a picker should be shown.
  virtual bool ShouldPromptBeforeDownload(
      const DlpFileDestination& download_src,
      const base::FilePath& file_path);

  // Checks whether launching `app_update` with `intent` is allowed.
  virtual void CheckIfLaunchAllowed(const apps::AppUpdate& app_update,
                                    apps::IntentPtr intent,
                                    CheckIfDlpAllowedCallback result_callback);

  // Returns true if `app_update` is blocked from opening any of the
  // files in `intent`.
  virtual bool IsLaunchBlocked(const apps::AppUpdate& app_update,
                               const apps::IntentPtr& intent);

  // Returns a sublist of `transferred_files` which aren't allowed to be
  // transferred to either `destination_url` or `destination_component` in
  // `result_callback`.
  virtual void IsFilesTransferRestricted(
      std::optional<file_manager::io_task::IOTaskId> task_id,
      const std::vector<FileDaemonInfo>& transferred_files,
      const DlpFileDestination& destination,
      dlp::FileAction files_action,
      IsFilesTransferRestrictedCallback result_callback);

  // Returns restriction information for `source_url`.
  virtual std::vector<DlpFileRestrictionDetails> GetDlpRestrictionDetails(
      const std::string& source_url);

  // Returns a list of components to which the transfer of a file with
  // `source_url` is blocked.
  virtual std::vector<data_controls::Component> GetBlockedComponents(
      const std::string& source_url);

  // Returns whether a dlp policy matches for the `file`.
  virtual bool IsDlpPolicyMatched(const FileDaemonInfo& file);

  //  VolumeManagerObserver overrides:
  void OnShutdownStart(file_manager::VolumeManager* volume_manager) override;

  DlpFilesEventStorage* GetEventStorageForTesting();

 protected:
  // DlpFilesController overrides:
  std::optional<data_controls::Component> MapFilePathToPolicyComponent(
      Profile* profile,
      const base::FilePath& file_path) override;

  bool IsInLocalFileSystem(const base::FilePath& file_path) override;

  void ShowDlpBlockedFiles(
      std::optional<file_manager::io_task::IOTaskId> task_id,
      std::vector<base::FilePath> blocked_files,
      dlp::FileAction action) override;

  // TODO(b/284122497): Cleanup friend for testing.
  FRIEND_TEST_ALL_PREFIXES(DlpFilesControllerAshComponentsTest,
                           MapFilePathToPolicyComponentTest);

  FRIEND_TEST_ALL_PREFIXES(DlpFilesControllerAshBlockUITest,
                           ShowDlpBlockedFiles);

 private:
  // Called back from warning dialog. Passes blocked files sources along
  // to |callback|. In case |should_proceed| is true, passes only
  // |files_levels|, otherwise passes also |warned_files_sources|.
  void OnDlpWarnDialogReply(
      std::vector<std::pair<FileDaemonInfo, ::dlp::RestrictionLevel>>
          files_levels,
      std::vector<FileDaemonInfo> warned_files_sources,
      std::vector<std::string> warned_src_patterns,
      std::vector<DlpRulesManager::RuleMetadata> warned_rules_metadata,
      const DlpFileDestination& dst,
      dlp::FileAction files_action,
      IsFilesTransferRestrictedCallback callback,
      std::optional<std::u16string> user_justification,
      bool should_proceed);

  void ReturnDisallowedFiles(
      std::optional<file_manager::io_task::IOTaskId> task_id,
      base::flat_map<std::string, storage::FileSystemURL> files_map,
      dlp::FileAction file_action,
      CheckIfTransferAllowedCallback result_callback,
      ::dlp::CheckFilesTransferResponse response);

  void ReturnAllowedUploads(std::vector<ui::SelectedFileInfo> uploaded_files,
                            FilterDisallowedUploadsCallback result_callback,
                            ::dlp::CheckFilesTransferResponse response);

  void ReturnDlpMetadata(const std::vector<storage::FileSystemURL>& files,
                         std::optional<DlpFileDestination> destination,
                         GetDlpMetadataCallback result_callback,
                         const ::dlp::GetFilesSourcesResponse response);

  // Reports an event if a `DlpReportingManager` instance exists. When
  // `level` is missing, we report a warning proceeded event.
  void MaybeReportEvent(ino64_t inode,
                        time_t crtime,
                        const base::FilePath& path,
                        const std::string& source_url,
                        const DlpFileDestination& dst,
                        const DlpRulesManager::RuleMetadata& rule_metadata,
                        std::optional<DlpRulesManager::Level> level);

  // Called when `transferred_files` is ready. Constructs CheckFilesTransfer
  // request and forwards it to the dlp daemon.
  void ContinueCheckIfTransferAllowed(
      std::optional<file_manager::io_task::IOTaskId> task_id,
      storage::FileSystemURL destination,
      bool is_move,
      CheckIfTransferAllowedCallback result_callback,
      std::vector<storage::FileSystemURL> transferred_files);

  // Called when `uploaded_files` is ready. Constructs CheckFilesTransfer
  // request and forwards it to the dlp daemon.
  void ContinueFilterDisallowedUploads(
      std::vector<ui::SelectedFileInfo> selected_files,
      const DlpFileDestination& destination,
      FilterDisallowedUploadsCallback result_callback,
      std::vector<storage::FileSystemURL> uploaded_files);

  // The profile with which we are associated. Not owned. It's currently always
  // the main/primary profile.
  const raw_ptr<Profile> profile_;

  // Keeps track of events and detects duplicate ones using time based
  // approach.
  std::unique_ptr<DlpFilesEventStorage> event_storage_;

  // Gets notified when an archive is extracted, and notifies the DLP daemon
  // about newly extracted files.
  std::unique_ptr<DlpExtractIOTaskObserver> extract_io_task_observer_;

  base::WeakPtrFactory<DlpFilesControllerAsh> weak_ptr_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_DLP_DLP_FILES_CONTROLLER_ASH_H_
