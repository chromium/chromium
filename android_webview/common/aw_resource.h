// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_COMMON_AW_RESOURCE_H_
#define ANDROID_WEBVIEW_COMMON_AW_RESOURCE_H_

#include <string>
#include <vector>

// android_webview implements these with a JNI call to the
// AwResource Java class.
namespace android_webview {
namespace AwResource {

std::vector<std::string> GetConfigKeySystemUuidMapping();

}  // namespace AwResource
}  // namsespace android_webview

#endif  // ANDROID_WEBVIEW_COMMON_AW_RESOURCE_H_
