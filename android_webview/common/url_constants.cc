// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/common/url_constants.h"

#include "base/strings/string_util.h"

namespace android_webview {

// These are special paths used with the file: scheme to access application
// assets and resources.
// See http://developer.android.com/reference/android/webkit/WebSettings.html
const char kAndroidAssetPath[] = "/android_asset/";
const char kAndroidResourcePath[] = "/android_res/";

// This scheme is used to display a default HTML5 video poster.
const char kAndroidWebViewVideoPosterScheme[] = "android-webview-video-poster";

bool IsAndroidSpecialFileUrl(const GURL& url) {
  if (!url.is_valid() || !url.SchemeIsFile() || !url.has_path())
    return false;
  return base::StartsWith(url.path(), kAndroidAssetPath,
                          base::CompareCase::SENSITIVE) ||
         base::StartsWith(url.path(), kAndroidResourcePath,
                          base::CompareCase::SENSITIVE);
}

}  // namespace android_webview
