// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/renderer/aw_content_settings_client.h"

#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "url/gurl.h"

namespace android_webview {

namespace {

bool AllowMixedContent(const blink::WebURL& url) {
  // We treat non-standard schemes as "secure" in the WebView to allow them to
  // be used for request interception.
  // TODO(benm): Tighten this restriction by requiring embedders to register
  // their custom schemes? See b/9420953.
  GURL gurl(url);
  return !gurl.IsStandard();
}

}  // namespace

AwContentSettingsClient::AwContentSettingsClient(
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame) {
  render_frame->GetWebFrame()->SetContentSettingsClient(this);
}

AwContentSettingsClient::~AwContentSettingsClient() {
}

bool AwContentSettingsClient::AllowRunningInsecureContent(
    bool enabled_per_settings,
    const blink::WebURL& url) {
  return enabled_per_settings ? true : AllowMixedContent(url);
}

bool AwContentSettingsClient::ShouldAutoupgradeMixedContent() {
  return render_frame()->GetBlinkPreferences().allow_mixed_content_upgrades;
}

void AwContentSettingsClient::OnDestruct() {
  delete this;
}

}  // namespace android_webview
