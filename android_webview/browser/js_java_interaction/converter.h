// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_JS_JAVA_INTERACTION_CONVERTER_H_
#define ANDROID_WEBVIEW_BROWSER_JS_JAVA_INTERACTION_CONVERTER_H_

#include "components/js_injection/common/interfaces.mojom-forward.h"
#include "third_party/blink/public/common/messaging/string_message_codec.h"

namespace android_webview {

// Converts |blink::WebMessagePayload| to |mojom::JsWebMessagePtr|.
js_injection::mojom::JsWebMessagePtr ConvertToJsWebMessagePtr(
    blink::WebMessagePayload payload);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_JS_JAVA_INTERACTION_CONVERTER_H_
