// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/network_service/aw_browser_context_io_thread_handle.h"

#include "content/public/browser/browser_thread.h"

namespace android_webview {

AwBrowserContextIoThreadHandle::AwBrowserContextIoThreadHandle(
    AwBrowserContext* browser_context)
    : browser_context_(browser_context) {
  DCHECK(browser_context_);
}

AwBrowserContextIoThreadHandle::~AwBrowserContextIoThreadHandle() = default;

AwBrowserContext* AwBrowserContextIoThreadHandle::GetOnUiThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return browser_context_;
}

std::unique_ptr<AwContentsIoThreadClient>
AwBrowserContextIoThreadHandle::GetServiceWorkerIoThreadClient() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  return browser_context_->GetServiceWorkerIoThreadClientThreadSafe();
}

}  // namespace android_webview
