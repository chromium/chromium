// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sms/sms_remote_fetcher.h"

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "chrome/browser/sharing/sharing_service_factory.h"
#include "chrome/browser/sharing/sms/sms_remote_fetcher_metrics.h"
#include "chrome/browser/sharing/sms/sms_remote_fetcher_ui_controller.h"
#include "content/public/browser/sms_fetcher.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"
#include "url/origin.h"

// TODO(crbug.com/40188157): Add browser tests for communication between this
// and the caller from content/.
base::OnceClosure FetchRemoteSms(
    content::WebContents* web_contents,
    const std::vector<url::Origin>& origin_list,
    base::OnceCallback<void(std::optional<std::vector<url::Origin>>,
                            std::optional<std::string>,
                            std::optional<content::SmsFetchFailureType>)>
        callback) {

  if (!SharingServiceFactory::GetForBrowserContext(
          web_contents->GetBrowserContext())) {
    std::move(callback).Run(std::nullopt, std::nullopt,
                            content::SmsFetchFailureType::kCrossDeviceFailure);
    RecordWebOTPCrossDeviceFailure(WebOTPCrossDeviceFailure::kNoSharingService);
    return base::NullCallback();
  }

// The current distinction of local fetcher being non-Android and remote fetcher
// being Android is a simplification we have made at this point and not a
// fundamental limitation. This may be relaxed in the future. e.g. allows
// tablets that run Android fetch a remote sms.
#if !BUILDFLAG(IS_ANDROID)
  auto* ui_controller =
      SmsRemoteFetcherUiController::GetOrCreateFromWebContents(web_contents);
  return ui_controller->FetchRemoteSms(origin_list, std::move(callback));
#else
  std::move(callback).Run(std::nullopt, std::nullopt,
                          content::SmsFetchFailureType::kCrossDeviceFailure);
  RecordWebOTPCrossDeviceFailure(
      WebOTPCrossDeviceFailure::kAndroidToAndroidNotSupported);
  return base::NullCallback();
#endif
}
