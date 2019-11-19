// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_ANDROID_PROTOCOL_HANDLER_H_
#define ANDROID_WEBVIEW_BROWSER_ANDROID_PROTOCOL_HANDLER_H_

#include <jni.h>
#include <memory>

class GURL;

namespace android_webview {
class InputStream;

std::unique_ptr<InputStream> CreateInputStream(JNIEnv* env, const GURL& url);

bool GetInputStreamMimeType(JNIEnv* env,
                            const GURL& url,
                            InputStream* stream,
                            std::string* mime_type);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_ANDROID_PROTOCOL_HANDLER_H_
