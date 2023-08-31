// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_FILES_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_FILES_CONTROLLER_H_

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/enterprise/data_controls/component.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/dbus/dlp/dlp_service.pb.h"
#include "components/file_access/scoped_file_access.h"
#include "storage/browser/file_system/file_system_url.h"

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

  DlpFilesController(const DlpFilesController& other) = delete;
  DlpFilesController& operator=(const DlpFilesController& other) = delete;

  // Requests ScopedFileAccess for |source| for the operation to copy from
  // |source| to |destination|.
  virtual void RequestCopyAccess(
      const storage::FileSystemURL& source,
      const storage::FileSystemURL& destination,
      base::OnceCallback<void(std::unique_ptr<file_access::ScopedFileAccess>)>
          result_callback);

  virtual ~DlpFilesController();

 protected:
  explicit DlpFilesController(const DlpRulesManager& rules_manager);

  virtual absl::optional<data_controls::Component> MapFilePathToPolicyComponent(
      Profile* profile,
      const base::FilePath& file_path) = 0;

  // TODO(b/284122497): Remove testing friend.
  FRIEND_TEST_ALL_PREFIXES(DlpFilesControllerComponentsTest, TestConvert);

  const raw_ref<const DlpRulesManager, DanglingUntriaged | ExperimentalAsh>
      rules_manager_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_FILES_CONTROLLER_H_
