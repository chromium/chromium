// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_STATUS_TEXT_BUILDER_UTILS_H_
#define CHROME_BROWSER_DOWNLOAD_STATUS_TEXT_BUILDER_UTILS_H_

#include <cstdint>
#include <string>

class StatusTextBuilderUtils {
 public:
  StatusTextBuilderUtils(const StatusTextBuilderUtils&) = delete;
  StatusTextBuilderUtils& operator=(const StatusTextBuilderUtils&) = delete;

  // Concatenates the `bytes_substring` and `detail_message` with a separator
  // with a down arrow prefix. Ex: "↓ 100/120 MB • Opening in 10 seconds..."
  static std::u16string GetActiveDownloadBubbleStatusMessageWithBytes(
      const std::u16string& bytes_substring,
      const std::u16string& detail_message);

  // Concatenates the `bytes_substring` and `detail_message` with a separator.
  // Ex: "100/120 MB • Opening in 10 seconds..."
  static std::u16string GetBubbleStatusMessageWithBytes(
      const std::u16string& bytes_substring,
      const std::u16string& detail_message);

  // Returns a string indicating the progress of an in-progress operation.
  // Ex: "100/120 MB"
  static std::u16string GetBubbleProgressSizesString(int64_t completed_bytes,
                                                     int64_t total_bytes);

  // Returns a string indicating the total size of a completed operation.
  // Ex: "100 MB • Done"
  static std::u16string GetCompletedTotalSizeString(int64_t total_bytes);
};

#endif  // CHROME_BROWSER_DOWNLOAD_STATUS_TEXT_BUILDER_UTILS_H_
