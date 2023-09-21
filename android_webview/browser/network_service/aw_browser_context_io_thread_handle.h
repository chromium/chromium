// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_NETWORK_SERVICE_AW_BROWSER_CONTEXT_IO_THREAD_HANDLE_H_
#define ANDROID_WEBVIEW_BROWSER_NETWORK_SERVICE_AW_BROWSER_CONTEXT_IO_THREAD_HANDLE_H_

#include "android_webview/browser/aw_browser_context.h"
#include "base/memory/ref_counted.h"

namespace android_webview {

class AwContentsIoThreadClient;

// This is an IO thread owned object that wraps an AwBrowserContext. The
// internal AwBrowserContext pointer can be gotten from the UI thread or the
// associated browser context's service worker IO thread client can be gotten
// on the IO thread.
class AwBrowserContextIoThreadHandle
    : public base::RefCountedThreadSafe<AwBrowserContextIoThreadHandle> {
 public:
  // Constructs a new AwBrowserContextIoThreadHandle wrapping the provided
  // AwBrowserContext.
  explicit AwBrowserContextIoThreadHandle(AwBrowserContext*);
  // Returns the internal pointer to the AwBrowserContext. Must only be called
  // from the UI thread.
  AwBrowserContext* GetOnUiThread();
  // Gets the service worker IO thread client associated with the wrapped
  // AwBrowserContext.
  std::unique_ptr<AwContentsIoThreadClient> GetServiceWorkerIoThreadClient();

 private:
  friend class base::RefCountedThreadSafe<AwBrowserContextIoThreadHandle>;
  ~AwBrowserContextIoThreadHandle();

  raw_ptr<AwBrowserContext> browser_context_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_NETWORK_SERVICE_AW_BROWSER_CONTEXT_IO_THREAD_HANDLE_H_
