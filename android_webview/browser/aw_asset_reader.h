// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_ASSET_READER_H_
#define ANDROID_WEBVIEW_BROWSER_AW_ASSET_READER_H_

#include <string>

#include "base/files/memory_mapped_file.h"

namespace android_webview {

class AwAssetReader {
 public:
  AwAssetReader() = default;

  int OpenApkAsset(const std::string& file_path,
                   base::MemoryMappedFile::Region* region);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_ASSET_READER_H_
