// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_TEXT_ATTACHMENT_H_
#define CHROME_BROWSER_NEARBY_SHARING_TEXT_ATTACHMENT_H_

#include <optional>
#include <string>

#include "chrome/browser/nearby_sharing/attachment.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_decoder_types.mojom.h"

// Represents a text attachment.
class TextAttachment : public Attachment {
 public:
  using Type = ::sharing::mojom::TextMetadata::Type;

  TextAttachment(Type type,
                 std::string text_body,
                 std::optional<std::string> text_title,
                 std::optional<std::string> mime_type);
  TextAttachment(int64_t id, Type type, std::string text_title, int64_t size);
  TextAttachment(const TextAttachment&);
  TextAttachment(TextAttachment&&);
  TextAttachment& operator=(const TextAttachment&);
  TextAttachment& operator=(TextAttachment&&);
  ~TextAttachment() override;

  const std::string& text_body() const { return text_body_; }
  const std::string& text_title() const { return text_title_; }
  Type type() const { return type_; }

  // Attachment:
  void MoveToShareTarget(ShareTarget& share_target) override;
  const std::string& GetDescription() const override;
  nearby_share::mojom::ShareType GetShareType() const override;

  void set_text_body(std::string text_body);

 private:
  Type type_;
  std::string text_title_;
  std::string text_body_;
  std::string mime_type_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_TEXT_ATTACHMENT_H_
