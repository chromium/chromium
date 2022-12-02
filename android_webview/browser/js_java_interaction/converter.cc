// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/js_java_interaction/converter.h"

#include "base/functional/overloaded.h"
#include "components/js_injection/common/interfaces.mojom.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/messaging/string_message_codec.h"

namespace android_webview {

js_injection::mojom::JsWebMessagePtr ConvertToJsWebMessagePtr(
    blink::WebMessagePayload payload) {
  return absl::visit(
      base::Overloaded{
          [](std::u16string& str) {
            return js_injection::mojom::JsWebMessage::NewStringValue(
                std::move(str));
          },
          [](std::unique_ptr<blink::WebMessageArrayBufferPayload>&
                 array_buffer) {
            mojo_base::BigBuffer big_buffer(array_buffer->GetLength());
            array_buffer->CopyInto(big_buffer);
            return js_injection::mojom::JsWebMessage::NewArrayBufferValue(
                std::move(big_buffer));
          }},
      payload);
}

}  // namespace android_webview
