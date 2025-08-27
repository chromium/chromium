// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/status_text_builder_utils.h"

#include <string>

#include "base/i18n/rtl.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/bytes_formatting.h"

namespace {

// Concatenates the `bytes_substring` and `detail_message` with a separator.
// Ex: "100/120 MB • Opening in 10 seconds..."
// If `is_download_active` is true, then the `bytes_substring` is prefixed with
// a down arrow symbol. Ex: "↓ 100/120 MB • Opening in 10 seconds..."
std::u16string BuildBubbleStatusMessageWithBytes(
    const std::u16string& bytes_substring,
    const std::u16string& detail_message,
    bool is_download_active) {
  // For some RTL languages (e.g. Hebrew), the translated form of 123/456 MB
  // still uses the English characters "MB" rather than RTL characters. We
  // specifically mark this as LTR because it should be displayed as "123/456
  // MB" (not "MB 123/456"). Conversely, some other RTL languages (e.g. Arabic)
  // do translate "MB" to RTL characters. For these, we do nothing, that way the
  // phrase is correctly displayed as RTL, with the translated "MB" to the left
  // of the fraction.
  std::u16string text;
  if (base::i18n::GetStringDirection(bytes_substring) ==
      base::i18n::TextDirection::LEFT_TO_RIGHT) {
    text = base::i18n::GetDisplayStringInLTRDirectionality(bytes_substring);
  } else {
    text = bytes_substring;
  }

  if (is_download_active) {
    text = l10n_util::GetStringFUTF16(
        IDS_DOWNLOAD_BUBBLE_DOWNLOAD_STATUS_WITH_SYMBOL, text);
  }

  text = l10n_util::GetStringFUTF16(
      IDS_DOWNLOAD_BUBBLE_DOWNLOAD_STATUS_MESSAGE_WITH_SEPARATOR, text,
      detail_message);

  // Some RTL languages like Hebrew still display "MB" in English
  // characters, which are the first strongly directional characters in
  // the full string. We mark the full string as RTL to ensure it doesn't get
  // displayed as LTR in spite of the first characters ("MB") being LTR.
  base::i18n::AdjustStringForLocaleDirection(&text);
  return text;
}

}  // namespace

// static
std::u16string
StatusTextBuilderUtils::GetActiveDownloadBubbleStatusMessageWithBytes(
    const std::u16string& bytes_substring,
    const std::u16string& detail_message) {
  return BuildBubbleStatusMessageWithBytes(bytes_substring, detail_message,
                                           /*is_download_active=*/true);
}

// static
std::u16string StatusTextBuilderUtils::GetBubbleStatusMessageWithBytes(
    const std::u16string& bytes_substring,
    const std::u16string& detail_message) {
  return BuildBubbleStatusMessageWithBytes(bytes_substring, detail_message,
                                           /*is_download_active=*/false);
}

// static
std::u16string StatusTextBuilderUtils::GetCompletedTotalSizeString(
    int64_t total_bytes) {
  return GetBubbleStatusMessageWithBytes(
      ui::FormatBytes(base::ByteCount(total_bytes)),
      l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_STATUS_DONE));
}

// static
std::u16string StatusTextBuilderUtils::GetBubbleProgressSizesString(
    int64_t completed_bytes,
    int64_t total_bytes) {
  base::ByteCount size = base::ByteCount(completed_bytes);
  base::ByteCount total = base::ByteCount(total_bytes);
  if (total > base::ByteCount(0)) {
    ui::DataUnits amount_units = ui::GetByteDisplayUnits(total);
    std::u16string simple_size =
        ui::FormatBytesWithUnits(size, amount_units, false);
    std::u16string simple_total =
        ui::FormatBytesWithUnits(total, amount_units, true);

    // Linux prepends an RLM (right-to-left mark) in the FormatBytesWithUnits
    // call when showing units if the string has strong RTL characters. This is
    // problematic for this fraction use case because it ends up moving it
    // around so that the numerator is in the wrong place. Therefore, we remove
    // that extra marker before proceeding.
    base::i18n::UnadjustStringForLocaleDirection(&simple_total);
    return l10n_util::GetStringFUTF16(IDS_DOWNLOAD_STATUS_SIZES, simple_size,
                                      simple_total);
  }
  return ui::FormatBytes(size);
}
