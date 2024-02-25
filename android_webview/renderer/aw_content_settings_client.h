// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_RENDERER_AW_CONTENT_SETTINGS_CLIENT_H_
#define ANDROID_WEBVIEW_RENDERER_AW_CONTENT_SETTINGS_CLIENT_H_

#include "content/public/renderer/render_frame_observer.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"

namespace android_webview {

// Android WebView implementation of blink::WebContentSettingsClient.
class AwContentSettingsClient : public content::RenderFrameObserver,
                                public blink::WebContentSettingsClient {
 public:
  explicit AwContentSettingsClient(content::RenderFrame* render_view);

  AwContentSettingsClient(const AwContentSettingsClient&) = delete;
  AwContentSettingsClient& operator=(const AwContentSettingsClient&) = delete;

 private:
  ~AwContentSettingsClient() override;

  // content::RenderFrameObserver implementation.
  void OnDestruct() override;

  // blink::WebContentSettingsClient implementation.
  bool AllowRunningInsecureContent(bool enabled_per_settings,
                                   const blink::WebURL& url) override;
  bool ShouldAutoupgradeMixedContent() override;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_RENDERER_AW_CONTENT_SETTINGS_CLIENT_H_
