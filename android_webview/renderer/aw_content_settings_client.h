// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_RENDERER_AW_CONTENT_SETTINGS_CLIENT_H_
#define ANDROID_WEBVIEW_RENDERER_AW_CONTENT_SETTINGS_CLIENT_H_

#include "base/macros.h"
#include "content/public/renderer/render_frame_observer.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"

namespace android_webview {

// Android WebView implementation of blink::WebContentSettingsClient.
class AwContentSettingsClient : public content::RenderFrameObserver,
                                public blink::WebContentSettingsClient {
 public:
  explicit AwContentSettingsClient(content::RenderFrame* render_view);

 private:
  ~AwContentSettingsClient() override;

  // content::RenderFrameObserver implementation.
  void OnDestruct() override;

  // blink::WebContentSettingsClient implementation.
  bool AllowRunningInsecureContent(bool enabled_per_settings,
                                   const blink::WebURL& url) override;

  DISALLOW_COPY_AND_ASSIGN(AwContentSettingsClient);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_RENDERER_AW_CONTENT_SETTINGS_CLIENT_H_
