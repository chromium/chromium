// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_SHARE_TARGET_H_
#define CHROME_BROWSER_NEARBY_SHARING_SHARE_TARGET_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "base/unguessable_token.h"
#include "chrome/browser/nearby_sharing/file_attachment.h"
#include "chrome/browser/nearby_sharing/text_attachment.h"
#include "chrome/browser/nearby_sharing/wifi_credentials_attachment.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_share_target_types.mojom.h"
#include "url/gurl.h"

// A remote device.
struct ShareTarget {
 public:
  ShareTarget();
  ShareTarget(
      std::string device_name,
      GURL image_url,
      nearby_share::mojom::ShareTargetType type,
      std::vector<TextAttachment> text_attachments,
      std::vector<FileAttachment> file_attachments,
      std::vector<WifiCredentialsAttachment> wifi_credentials_attachments,
      bool is_incoming,
      std::optional<std::string> full_name,
      bool is_known,
      std::optional<std::string> device_id,
      bool for_self_share);
  ShareTarget(const ShareTarget&);
  ShareTarget(ShareTarget&&);
  ShareTarget& operator=(const ShareTarget&);
  ShareTarget& operator=(ShareTarget&&);
  ~ShareTarget();

  bool has_attachments() const {
    return !text_attachments.empty() || !file_attachments.empty() ||
           !wifi_credentials_attachments.empty();
  }

  std::vector<int64_t> GetAttachmentIds() const;

  // Returns whether the ShareTarget can auto-accept the transfer, which
  // is used for Self Share.
  bool CanAutoAccept() const;

  base::UnguessableToken id = base::UnguessableToken::Create();
  std::string device_name;
  // Uri that points to an image of the ShareTarget, if one exists.
  std::optional<GURL> image_url;
  nearby_share::mojom::ShareTargetType type =
      nearby_share::mojom::ShareTargetType::kUnknown;
  std::vector<TextAttachment> text_attachments;
  std::vector<FileAttachment> file_attachments;
  std::vector<WifiCredentialsAttachment> wifi_credentials_attachments;
  bool is_incoming = false;
  std::optional<std::string> full_name;
  // True if local device has the PublicCertificate this target is advertising.
  bool is_known = false;
  std::optional<std::string> device_id;
  // True if the remote device is also owned by the current user.
  bool for_self_share = false;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_SHARE_TARGET_H_
