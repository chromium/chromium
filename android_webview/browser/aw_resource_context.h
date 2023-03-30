// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_RESOURCE_CONTEXT_H_
#define ANDROID_WEBVIEW_BROWSER_AW_RESOURCE_CONTEXT_H_

#include "content/public/browser/resource_context.h"

namespace android_webview {

class AwResourceContext : public content::ResourceContext {
 public:
  AwResourceContext();

  AwResourceContext(const AwResourceContext&) = delete;
  AwResourceContext& operator=(const AwResourceContext&) = delete;

  ~AwResourceContext() override;

 private:
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_RESOURCE_CONTEXT_H_
