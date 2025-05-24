// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/local_image_search/local_image_search_test_util.h"

#include "chrome/browser/ash/app_list/search/local_image_search/annotation_storage.h"
#include "chrome/browser/ash/app_list/search/local_image_search/file_search_result.h"

namespace app_list {

bool operator==(const AnnotationInfo& i1, const AnnotationInfo& i2) {
  return i1.score == i2.score && i1.x == i2.x && i1.y == i2.y &&
         i1.area == i2.area;
}

bool operator==(const ImageInfo& i1, const ImageInfo& i2) {
  return i1.path == i2.path && i1.annotations == i2.annotations &&
         i1.annotation_map == i2.annotation_map &&
         i1.last_modified == i2.last_modified && i1.file_size == i2.file_size;
}

bool operator==(const ImageStatus& i1, const ImageStatus& i2) {
  return i1.last_modified == i2.last_modified &&
         i1.ocr_version == i2.ocr_version && i1.ica_version == i2.ica_version;
}

bool operator==(const FileSearchResult& f1, const FileSearchResult& f2) {
  return f1.file_path == f2.file_path && f1.last_modified == f2.last_modified &&
         std::abs(f1.relevance - f2.relevance) < 0.001;
}

}  // namespace app_list
