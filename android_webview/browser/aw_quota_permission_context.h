// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_QUOTA_PERMISSION_CONTEXT_H_
#define ANDROID_WEBVIEW_BROWSER_AW_QUOTA_PERMISSION_CONTEXT_H_

#include "base/compiler_specific.h"
#include "content/public/browser/quota_permission_context.h"

namespace android_webview {

class AwQuotaPermissionContext : public content::QuotaPermissionContext {
 public:
  AwQuotaPermissionContext();

  AwQuotaPermissionContext(const AwQuotaPermissionContext&) = delete;
  AwQuotaPermissionContext& operator=(const AwQuotaPermissionContext&) = delete;

  void RequestQuotaPermission(const content::StorageQuotaParams& params,
                              int render_process_id,
                              PermissionCallback callback) override;

 private:
  ~AwQuotaPermissionContext() override;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_QUOTA_PERMISSION_CONTEXT_H_

