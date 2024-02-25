// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/share_target.h"

#include <utility>

ShareTarget::ShareTarget() = default;

ShareTarget::ShareTarget(
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
    bool for_self_share)
    : device_name(std::move(device_name)),
      image_url(std::move(image_url)),
      type(type),
      text_attachments(std::move(text_attachments)),
      file_attachments(std::move(file_attachments)),
      wifi_credentials_attachments(std::move(wifi_credentials_attachments)),
      is_incoming(is_incoming),
      full_name(std::move(full_name)),
      is_known(is_known),
      device_id(std::move(device_id)),
      for_self_share(for_self_share) {}

ShareTarget::ShareTarget(const ShareTarget&) = default;

ShareTarget::ShareTarget(ShareTarget&&) = default;

ShareTarget& ShareTarget::operator=(const ShareTarget&) = default;

ShareTarget& ShareTarget::operator=(ShareTarget&&) = default;

ShareTarget::~ShareTarget() = default;

std::vector<int64_t> ShareTarget::GetAttachmentIds() const {
  std::vector<int64_t> attachment_ids;

  for (const auto& file : file_attachments)
    attachment_ids.push_back(file.id());

  for (const auto& text : text_attachments)
    attachment_ids.push_back(text.id());

  for (const auto& wifi_credentials : wifi_credentials_attachments) {
    attachment_ids.push_back(wifi_credentials.id());
  }

  return attachment_ids;
}

bool ShareTarget::CanAutoAccept() const {
  // Only Self Shares can be auto accepted. Additionally, Wi-Fi credentials
  // cannot be auto accepted due to the security risk.
  return for_self_share && wifi_credentials_attachments.empty();
}
