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
#include "url/gurl.h"

namespace storage {
class FileSystemURL;
}  // namespace storage

class Profile;

namespace policy {

// DlpFilesController is responsible for deciding whether file transfers are
// allowed according to the files sources saved in the DLP daemon and the rules
// of the Data leak prevention policy set by the admin.
class DlpFilesController {
 public:
  using GetDisallowedTransfersCallback =
      base::OnceCallback<void(std::vector<storage::FileSystemURL>)>;
  using GetFilesRestrictedByAnyRuleCallback = GetDisallowedTransfersCallback;

  DlpFilesController();

  DlpFilesController(const DlpFilesController& other) = delete;
  DlpFilesController& operator=(const DlpFilesController& other) = delete;

  ~DlpFilesController();

  // Returns a list of files disallowed to be transferred in |result_callback|.
  void GetDisallowedTransfers(
      std::vector<storage::FileSystemURL> transferred_files,
      storage::FileSystemURL destination,
      GetDisallowedTransfersCallback result_callback);

  // Returns a list of files restricted by any DLP rule in |result_callback|.
  void GetFilesRestrictedByAnyRule(
      std::vector<storage::FileSystemURL> files,
      GetFilesRestrictedByAnyRuleCallback result_callback);

  // Returns a list of `files_sources` from from which files aren't allowed to
  // be transferred to `destination`.
  static std::vector<GURL> IsFilesTransferRestricted(
      Profile* profile,
      std::vector<GURL> files_sources,
      std::string destination);

 private:
  void OnCheckFilesTransferReply(
      base::flat_map<std::string, storage::FileSystemURL> files_map,
      GetDisallowedTransfersCallback result_callback,
      const dlp::CheckFilesTransferResponse response);

  base::WeakPtrFactory<DlpFilesController> weak_ptr_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_DLP_DLP_FILES_CONTROLLER_H_
