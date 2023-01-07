// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_RESOURCE_CONTEXT_H_
#define ANDROID_WEBVIEW_BROWSER_AW_RESOURCE_CONTEXT_H_

#include <map>
#include <string>

#include "base/synchronization/lock.h"
#include "content/public/browser/resource_context.h"

class GURL;

namespace android_webview {

class AwResourceContext : public content::ResourceContext {
 public:
  AwResourceContext();

  AwResourceContext(const AwResourceContext&) = delete;
  AwResourceContext& operator=(const AwResourceContext&) = delete;

  ~AwResourceContext() override;

  void SetExtraHeaders(const GURL& url, const std::string& headers);
  std::string GetExtraHeaders(const GURL& url);

 private:
  base::Lock extra_headers_lock_;
  std::map<std::string, std::string> extra_headers_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_RESOURCE_CONTEXT_H_
