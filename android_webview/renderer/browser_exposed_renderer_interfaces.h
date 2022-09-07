// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_RENDERER_BROWSER_EXPOSED_RENDERER_INTERFACES_H_
#define ANDROID_WEBVIEW_RENDERER_BROWSER_EXPOSED_RENDERER_INTERFACES_H_

namespace mojo {
class BinderMap;
}  // namespace mojo

namespace android_webview {

class AwContentRendererClient;

void ExposeRendererInterfacesToBrowser(AwContentRendererClient* client,
                                       mojo::BinderMap* binders);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_RENDERER_BROWSER_EXPOSED_RENDERER_INTERFACES_H_
