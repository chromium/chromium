// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/renderer/aw_print_render_frame_helper_delegate.h"

#include "third_party/blink/public/web/web_element.h"

namespace android_webview {

AwPrintRenderFrameHelperDelegate::AwPrintRenderFrameHelperDelegate() = default;

AwPrintRenderFrameHelperDelegate::~AwPrintRenderFrameHelperDelegate() = default;

blink::WebElement AwPrintRenderFrameHelperDelegate::GetPdfElement(
    blink::WebLocalFrame* frame) {
  return blink::WebElement();
}

bool AwPrintRenderFrameHelperDelegate::IsPrintPreviewEnabled() {
  return false;
}

bool AwPrintRenderFrameHelperDelegate::IsScriptedPrintEnabled() {
  return false;
}

bool AwPrintRenderFrameHelperDelegate::OverridePrint(
    blink::WebLocalFrame* frame) {
  return false;
}

}  // namespace android_webview
