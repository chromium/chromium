// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_ORIGIN_VERIFICATION_SCHEDULER_BRIDGE_H_
#define ANDROID_WEBVIEW_BROWSER_AW_ORIGIN_VERIFICATION_SCHEDULER_BRIDGE_H_

#include "base/functional/callback.h"
#include "base/no_destructor.h"

#include "components/digital_asset_links/browser_url_loader_throttle.h"

namespace android_webview {
using OriginVerifierCallback = base::OnceCallback<void(bool /*verified*/)>;

class AwOriginVerificationSchedulerBridge
    : public digital_asset_links::BrowserURLLoaderThrottle::
          OriginVerificationSchedulerBridge {
 public:
  static AwOriginVerificationSchedulerBridge* GetInstance();

  AwOriginVerificationSchedulerBridge(
      const AwOriginVerificationSchedulerBridge&) = delete;
  AwOriginVerificationSchedulerBridge& operator=(
      const AwOriginVerificationSchedulerBridge&) = delete;

  void Verify(std::string url, OriginVerifierCallback callback) override;

 private:
  AwOriginVerificationSchedulerBridge() = default;
  ~AwOriginVerificationSchedulerBridge() override = default;

  friend class base::NoDestructor<AwOriginVerificationSchedulerBridge>;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_ORIGIN_VERIFICATION_SCHEDULER_BRIDGE_H_
