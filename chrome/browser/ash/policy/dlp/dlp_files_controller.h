// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_DLP_DLP_FILES_CONTROLLER_H_
#define CHROME_BROWSER_ASH_POLICY_DLP_DLP_FILES_CONTROLLER_H_

#include <vector>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/dlp/dlp_service.pb.h"

namespace storage {
class FileSystemURL;
}  // namespace storage

class Profile;

namespace policy {

class DlpRulesManager;

// DlpFilesController is responsible for deciding whether file transfers are
// allowed according to the files sources saved in the DLP daemon and the rules
// of the Data leak prevention policy set by the admin.
class DlpFilesController {
 public:
  using GetDisallowedTransfersCallback =
      base::OnceCallback<void(std::vector<storage::FileSystemURL>)>;

  // `dlp_rules_manager` must outlive this class.
  explicit DlpFilesController(Profile* profile,
                              DlpRulesManager* dlp_rules_manager);

  DlpFilesController(const DlpFilesController& other) = delete;
  DlpFilesController& operator=(const DlpFilesController& other) = delete;

  ~DlpFilesController();

  // Returns a list of files disallowed to be transferred in |result_callback|.
  void GetDisallowedTransfers(
      std::vector<storage::FileSystemURL> transferred_files,
      storage::FileSystemURL destination,
      GetDisallowedTransfersCallback result_callback);

 private:
  void GetFilesSources(storage::FileSystemURL destination,
                       GetDisallowedTransfersCallback result_callback,
                       base::flat_map<ino_t, storage::FileSystemURL> files_map);
  void OnGetFilesSourcesReply(
      base::flat_map<ino_t, storage::FileSystemURL> files_map,
      storage::FileSystemURL destination,
      GetDisallowedTransfersCallback result_callback,
      const dlp::GetFilesSourcesResponse response);

  Profile* profile_;  // Unowned.

  DlpRulesManager* dlp_rules_manager_;  // Unowned.

  base::WeakPtrFactory<DlpFilesController> weak_ptr_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_DLP_DLP_FILES_CONTROLLER_H_
