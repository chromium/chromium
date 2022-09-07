// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_ANDROID_PROTOCOL_HANDLER_H_
#define ANDROID_WEBVIEW_BROWSER_ANDROID_PROTOCOL_HANDLER_H_

#include <jni.h>
#include <memory>

class GURL;

namespace embedder_support {
class InputStream;
}

namespace android_webview {

std::unique_ptr<embedder_support::InputStream> CreateInputStream(
    JNIEnv* env,
    const GURL& url);

bool GetInputStreamMimeType(JNIEnv* env,
                            const GURL& url,
                            embedder_support::InputStream* stream,
                            std::string* mime_type);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_ANDROID_PROTOCOL_HANDLER_H_
