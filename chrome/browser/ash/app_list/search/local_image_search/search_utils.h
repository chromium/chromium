// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGE_SEARCH_SEARCH_UTILS_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGE_SEARCH_SEARCH_UTILS_H_

#include <string>
#include <vector>

namespace app_list {

// The current indexing version of ica and ocr. An image re-indexing will be
// required of the current version is later than the version of image in
// database.
inline constexpr int kOcrVersion = 1;
inline constexpr int kIcaVersion = 1;

// Which indexing source is an image annotation result coming from.
enum class IndexingSource {
  kOcr,  // Optical character recognition for texts within the image.
  kIca,  // Image content search for contents within the image.
};

struct FileSearchResult;

// Returns sorted `FileSearchResult`s contained in both sorted arrays.
std::vector<FileSearchResult> FindIntersection(
    const std::vector<FileSearchResult>& vec1,
    const std::vector<FileSearchResult>& vec2);

// Checks for the `word` in the current list of stop words.
bool IsStopWord(const std::string& word);

// These values persist to logs. Entries should not be renumbered and numeric
// values should never be reused.
enum class IcaStatus {
  kStartWithOcr = 0,
  kStartWithoutOcr = 1,
  kOcrSucceed = 2,
  kIcaSucceed = 3,
  kOcrInserted = 4,
  kIcaInserted = 5,
  kIcaFailed = 6,
  kIcaDisabled = 7,
  kAnnotateStart = 8,
  kDataInitFailed = 9,
  kMappedRegionInvalid = 10,
  kRequestSent = 11,
  kTimeout = 12,
  kOcrInsertStart = 13,
  kIcaInsertStart = 14,
  kOcrDocumentInsertFailed = 15,
  kIcaDocumentInsertFailed = 16,
  kOcrAnnotationInsertFailed = 17,
  kIcaAnnotationInsertFailed = 18,
  kOcrUpdateFailed = 19,
  kIcaUpdateFailed = 20,
  kMaxValue = kIcaUpdateFailed,
};

void LogIcaUma(IcaStatus status);

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGE_SEARCH_SEARCH_UTILS_H_
