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

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGE_SEARCH_SEARCH_UTILS_H_
