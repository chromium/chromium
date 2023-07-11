// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGE_SEARCH_LOCAL_IMAGE_SEARCH_TEST_UTIL_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGE_SEARCH_LOCAL_IMAGE_SEARCH_TEST_UTIL_H_

namespace app_list {

struct ImageInfo;
struct FileSearchResult;

bool operator==(const ImageInfo& i1, const ImageInfo& i2);

bool operator==(const FileSearchResult& f1, const FileSearchResult& f2);

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGE_SEARCH_LOCAL_IMAGE_SEARCH_TEST_UTIL_H_
