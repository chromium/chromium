// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_media_url_interceptor.h"

#include <string>

#include "android_webview/browser/aw_asset_reader.h"
#include "android_webview/common/url_constants.h"
#include "base/android/apk_assets.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "content/public/common/url_constants.h"

namespace android_webview {

bool AwMediaUrlInterceptor::Intercept(const std::string& url,
                                      int* fd,
                                      int64_t* offset,
                                      int64_t* size) const {
  std::string asset_file_prefix(url::kFileScheme);
  asset_file_prefix.append(url::kStandardSchemeSeparator);
  asset_file_prefix.append(android_webview::kAndroidAssetPath);

  if (base::StartsWith(url, asset_file_prefix, base::CompareCase::SENSITIVE)) {
    std::string filename(url);
    base::ReplaceFirstSubstringAfterOffset(&filename, 0, asset_file_prefix,
                                           "assets/");
    base::MemoryMappedFile::Region region =
        base::MemoryMappedFile::Region::kWholeFile;
    std::unique_ptr<AwAssetReader> reader;
    *fd = reader->OpenApkAsset(filename, &region);
    *offset = region.offset;
    *size = region.size;
    base::UmaHistogramBoolean("Android.WebView.MediaUrlIntercept", *fd != -1);
    return *fd != -1;
  }

  base::UmaHistogramBoolean("Android.WebView.MediaUrlIntercept", false);
  return false;
}

}  // namespace android_webview
