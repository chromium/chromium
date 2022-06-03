// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_web_contents_view_delegate.h"

#include "content/public/browser/web_contents.h"
#include "ui/gfx/color_space.h"

namespace android_webview {

// static
content::WebContentsViewDelegate* AwWebContentsViewDelegate::Create(
    content::WebContents* web_contents) {
  return new AwWebContentsViewDelegate(web_contents);
}

AwWebContentsViewDelegate::AwWebContentsViewDelegate(
    content::WebContents* web_contents) {
  // Cannot instantiate web_contents_view_delegate_ here because
  // AwContents::SetWebDelegate is not called yet.
}

AwWebContentsViewDelegate::~AwWebContentsViewDelegate() {}

content::WebDragDestDelegate* AwWebContentsViewDelegate::GetDragDestDelegate() {
  // GetDragDestDelegate is a pure virtual method from WebContentsViewDelegate
  // and must have an implementation although android doesn't use it.
  NOTREACHED();
  return NULL;
}

}  // namespace android_webview
