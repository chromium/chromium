// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_FILES_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_FILES_UTILS_H_

#include "chrome/browser/chromeos/policy/dlp/dlp_policy_constants.h"
#include "chromeos/dbus/dlp/dlp_service.pb.h"
#include "components/enterprise/data_controls/core/browser/component.h"
#include "url/gurl.h"

namespace policy {
namespace dlp {

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

// Maps |component| to ::dlp::DlpComponent.
::dlp::DlpComponent MapPolicyComponentToProto(
    data_controls::Component component);

// Returns whether there is at least one source in `sources` for which the
// transfer to `component` is blocked. The vector `sources` may contain empty
// strings for unmanaged files.
bool IsFilesTransferBlocked(const std::vector<std::string>& sources,
                            data_controls::Component component);

// Opens the policy Learn more page.
void OpenLearnMore(const GURL& url = GURL(dlp::kDlpLearnMoreUrl));

}  // namespace dlp
}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_FILES_UTILS_H_
