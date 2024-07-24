// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_FILES_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_FILES_CONTROLLER_H_

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_file_destination.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_utils.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/dbus/dlp/dlp_service.pb.h"
#include "components/enterprise/data_controls/core/browser/component.h"
#include "components/file_access/scoped_file_access.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/recursive_operation_delegate.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"

namespace policy {

// DlpFilesController is responsible for deciding whether file transfers are
// allowed according to the files sources saved in the DLP daemon and the rules
// of the Data leak prevention policy set by the admin.
class DlpFilesController {
 public:
  // FileDaemonInfo represents file info used for communication with the DLP
  // daemon.
  struct FileDaemonInfo {
    FileDaemonInfo() = delete;
    FileDaemonInfo(ino64_t inode,
                   time_t crtime,
                   const base::FilePath& path,
                   const std::string& source_url,
                   const std::string& referrer_url);
    FileDaemonInfo(const FileDaemonInfo&);

    friend bool operator==(const FileDaemonInfo& a, const FileDaemonInfo& b) {
      return a.inode == b.inode && a.crtime == b.crtime && a.path == b.path &&
             a.source_url == b.source_url && a.referrer_url == b.referrer_url;
    }
    friend bool operator!=(const FileDaemonInfo& a, const FileDaemonInfo& b) {
      return !(a == b);
    }

    // File inode.
    ino64_t inode;
    // File creation time.
    time_t crtime;
    // File path.
    base::FilePath path;
    // Source URL from which the file was downloaded.
    GURL source_url;
    // Referrer URL from which the download process was initiated.
    GURL referrer_url;
  };

  // Gets all files inside |root| recursively and runs |callback_| with the
  // files list.
  class FolderRecursionDelegate final
      : public storage::RecursiveOperationDelegate {
   public:
    using FileURLsCallback =
        base::OnceCallback<void(std::vector<storage::FileSystemURL>)>;

    FolderRecursionDelegate(storage::FileSystemContext* file_system_context,
                            const storage::FileSystemURL& root,
                            FileURLsCallback callback);

    FolderRecursionDelegate(const FolderRecursionDelegate&) = delete;
    FolderRecursionDelegate& operator=(const FolderRecursionDelegate&) = delete;

    ~FolderRecursionDelegate() override;

    // RecursiveOperationDelegate:
    void Run() override;
    void RunRecursively() override;
    void ProcessFile(const storage::FileSystemURL& url,
                     StatusCallback callback) override;
    void ProcessDirectory(const storage::FileSystemURL& url,
                          StatusCallback callback) override;
    void PostProcessDirectory(const storage::FileSystemURL& url,
                              StatusCallback callback) override;
    base::WeakPtr<storage::RecursiveOperationDelegate> AsWeakPtr() override;

   private:
    void OnGetMetadata(const storage::FileSystemURL& url,
                       StatusCallback callback,
                       base::File::Error result,
                       const base::File::Info& file_info);

    void Completed(base::File::Error result);

    const raw_ref<const storage::FileSystemURL> root_;
    FileURLsCallback callback_;
    std::vector<storage::FileSystemURL> files_urls_;

    base::WeakPtrFactory<FolderRecursionDelegate> weak_ptr_factory_{this};
  };

  // Gets all files inside |roots| recursively and runs |callback_| with the
  // whole files list. Deletes itself after |callback_| is run.
  // TODO(b/259184140): Extract RootsRecursionDelegate to another file to
  // have better testing coverage.
  class RootsRecursionDelegate {
   public:
    RootsRecursionDelegate(storage::FileSystemContext* file_system_context,
                           std::vector<storage::FileSystemURL> roots,
                           FolderRecursionDelegate::FileURLsCallback callback);

    RootsRecursionDelegate(const RootsRecursionDelegate&) = delete;
    RootsRecursionDelegate& operator=(const RootsRecursionDelegate&) = delete;

    ~RootsRecursionDelegate();

    // Starts getting all files inside |roots| recursively.
    void Run();

    // Runs |callback_| when all files are ready.
    void Completed(std::vector<storage::FileSystemURL> files_urls);

   private:
    // Counts the number of |roots| processed.
    uint counter_ = 0;
    raw_ptr<storage::FileSystemContext, DanglingUntriaged>
        file_system_context_ = nullptr;
    const std::vector<storage::FileSystemURL> roots_;
    FolderRecursionDelegate::FileURLsCallback callback_;
    std::vector<storage::FileSystemURL> files_urls_;
    std::vector<std::unique_ptr<FolderRecursionDelegate>> delegates_;

    base::WeakPtrFactory<RootsRecursionDelegate> weak_ptr_factory_{this};
  };

  using CheckIfDlpAllowedCallback = base::OnceCallback<void(bool is_allowed)>;

  DlpFilesController(const DlpFilesController& other) = delete;
  DlpFilesController& operator=(const DlpFilesController& other) = delete;
  virtual ~DlpFilesController();

  // Requests ScopedFileAccess for |source| for the operation to copy from
  // |source| to |destination|.
  virtual void RequestCopyAccess(
      const storage::FileSystemURL& source,
      const storage::FileSystemURL& destination,
      base::OnceCallback<void(std::unique_ptr<file_access::ScopedFileAccess>)>
          result_callback);

  // Checks whether pasting or dropping the given `paths` to `data_dst` is
  // allowed.
  virtual void CheckIfPasteOrDropIsAllowed(
      const std::vector<base::FilePath>& files,
      const ui::DataTransferEndpoint* data_dst,
      CheckIfDlpAllowedCallback result_callback);

  // Returns a testing FileSystemContext* if set, otherwise it returns
  // FileSystemContext* for the primary profile.
  storage::FileSystemContext* GetFileSystemContextForPrimaryProfile();

  void SetFileSystemContextForTesting(
      storage::FileSystemContext* file_system_context);

 protected:
  explicit DlpFilesController(const DlpRulesManager& rules_manager);

  // Maps |file_path| to data_controls::Component if possible.
  virtual std::optional<data_controls::Component> MapFilePathToPolicyComponent(
      Profile* profile,
      const base::FilePath& file_path) = 0;

  // Shows DLP block desktop notification.
  virtual void ShowDlpBlockedFiles(std::optional<uint64_t> task_id,
                                   std::vector<base::FilePath> blocked_files,
                                   dlp::FileAction action) = 0;

  // Returns true if `file_path` is in MyFiles directory.
  virtual bool IsInLocalFileSystem(const base::FilePath& file_path) = 0;

  // Checks whether pasting or dropping the given `files` to `destination` is
  // allowed by constructing a CheckFilesTransfer request that is forwarded  to
  // the DLP daemon.
  void ContinueCheckIfPasteOrDropIsAllowed(
      const DlpFileDestination& destination,
      CheckIfDlpAllowedCallback result_callback,
      std::vector<storage::FileSystemURL> files);

  // Runs `result_callback` with true if `action` is allowed. It runs
  // `result_callback` with false and shows the required UI otherwise.
  void ReturnIfActionAllowed(dlp::FileAction action,
                             CheckIfDlpAllowedCallback result_callback,
                             ::dlp::CheckFilesTransferResponse response);

  // TODO(b/284122497): Remove testing friend.
  FRIEND_TEST_ALL_PREFIXES(DlpFilesControllerComponentsTest, TestConvert);

  const raw_ref<const DlpRulesManager, DanglingUntriaged> rules_manager_;

  base::WeakPtrFactory<DlpFilesController> weak_ptr_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_FILES_CONTROLLER_H_
