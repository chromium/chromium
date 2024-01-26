// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <utility>

#include "base/strings/strcat.h"
#include "chrome/browser/nearby_sharing/share_target.h"
#include "chrome/browser/nearby_sharing/text_attachment.h"
#include "components/drive/drive_api_util.h"
#include "url/gurl.h"

namespace {

// Tries to get a valid host name from the |text|. Returns nullopt otherwise.
std::optional<std::string> GetHostFromText(const std::string& text) {
  GURL url(text);
  if (!url.is_valid() || !url.has_host())
    return std::nullopt;

  return url.host();
}

// Masks the given |number| depending on the string length:
//  - length <= 4: Masks all characters
//  - 4 < length <= 6 Masks the last 4 characters
//  - 6 < length <= 10: Skips the first 2 and masks the following 4 characters
//  - length > 10: Skips the first 2 and last 4 characters. Masks the rest of
//    the string
// Note: We're assuming a formatted phone number and won't try to reformat to
// E164 like on Android as there's no easy way of determining the intended
// region for the phone number.
std::string MaskPhoneNumber(const std::string& number) {
  constexpr int kMinMaskedDigits = 4;
  constexpr int kMaxLeadingDigits = 2;
  constexpr int kMaxTailingDigits = 4;
  constexpr int kLengthWithNoLeadingDigits = kMinMaskedDigits;
  constexpr int kLengthWithNoTailingDigits =
      kMinMaskedDigits + kMaxLeadingDigits;

  if (number.empty())
    return number;

  std::string result = number;
  bool has_plus = false;
  if (number[0] == '+') {
    result = number.substr(1);
    has_plus = true;
  }

  // First calculate how many digits we would mask having exactly
  // kMinMaskedDigits digits masked.
  int leading_digits = 0;
  if (result.length() > kLengthWithNoLeadingDigits)
    leading_digits = result.length() - kLengthWithNoLeadingDigits;

  int tailing_digits = 0;
  if (result.length() > kLengthWithNoTailingDigits)
    tailing_digits = result.length() - kLengthWithNoTailingDigits;

  // Now limit resulting numbers of digits to maximally allowed values.
  leading_digits = std::min(kMaxLeadingDigits, leading_digits);
  tailing_digits = std::min(kMaxTailingDigits, tailing_digits);
  int masked_digits = result.length() - leading_digits - tailing_digits;

  return base::StrCat({(has_plus ? "+" : ""), result.substr(0, leading_digits),
                       std::string(masked_digits, 'x'),
                       result.substr(result.length() - tailing_digits)});
}

std::string GetTextTitle(const std::string& text_body,
                         TextAttachment::Type type) {
  constexpr size_t kMaxPreviewTextLength = 32;

  switch (type) {
    case TextAttachment::Type::kUrl: {
      std::optional<std::string> host = GetHostFromText(text_body);
      if (host)
        return *host;

      break;
    }
    case TextAttachment::Type::kPhoneNumber:
      return MaskPhoneNumber(text_body);
    default:
      break;
  }

  if (text_body.size() > kMaxPreviewTextLength)
    return base::StrCat({text_body.substr(0, kMaxPreviewTextLength), "…"});

  return text_body;
}

}  // namespace

TextAttachment::TextAttachment(Type type,
                               std::string text_body,
                               std::optional<std::string> text_title,
                               std::optional<std::string> mime_type)
    : Attachment(Attachment::Family::kText, text_body.size()),
      type_(type),
      text_title_(text_title && !text_title->empty()
                      ? *text_title
                      : GetTextTitle(text_body, type)),
      text_body_(std::move(text_body)),
      mime_type_(mime_type ? *mime_type : std::string()) {}

TextAttachment::TextAttachment(int64_t id,
                               Type type,
                               std::string text_title,
                               int64_t size)
    : Attachment(id, Attachment::Family::kText, size),
      type_(type),
      text_title_(std::move(text_title)) {}

TextAttachment::TextAttachment(const TextAttachment&) = default;

TextAttachment::TextAttachment(TextAttachment&&) = default;

TextAttachment& TextAttachment::operator=(const TextAttachment&) = default;

TextAttachment& TextAttachment::operator=(TextAttachment&&) = default;

TextAttachment::~TextAttachment() = default;

void TextAttachment::MoveToShareTarget(ShareTarget& share_target) {
  share_target.text_attachments.push_back(std::move(*this));
}

const std::string& TextAttachment::GetDescription() const {
  return text_title_;
}

nearby_share::mojom::ShareType TextAttachment::GetShareType() const {
  switch (type()) {
    case TextAttachment::Type::kUrl:
      if (mime_type_ == drive::util::kGoogleDocumentMimeType) {
        return nearby_share::mojom::ShareType::kGoogleDocsFile;
      } else if (mime_type_ == drive::util::kGoogleSpreadsheetMimeType) {
        return nearby_share::mojom::ShareType::kGoogleSheetsFile;
      } else if (mime_type_ == drive::util::kGooglePresentationMimeType) {
        return nearby_share::mojom::ShareType::kGoogleSlidesFile;
      } else {
        return nearby_share::mojom::ShareType::kUrl;
      }
    case TextAttachment::Type::kAddress:
      return nearby_share::mojom::ShareType::kAddress;
    case TextAttachment::Type::kPhoneNumber:
      return nearby_share::mojom::ShareType::kPhone;
    default:
      return nearby_share::mojom::ShareType::kText;
  }
}

void TextAttachment::set_text_body(std::string text_body) {
  text_body_ = std::move(text_body);
  text_title_ = GetTextTitle(text_body_, type_);
}
