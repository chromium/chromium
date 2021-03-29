// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sms/sms_remote_fetcher.h"

#include "base/check.h"
#include "build/build_config.h"
#include "chrome/browser/sharing/sms/sms_flags.h"
#include "chrome/browser/sharing/sms/sms_remote_fetcher_ui_controller.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"
#include "url/origin.h"

base::OnceClosure FetchRemoteSms(
    content::WebContents* web_contents,
    const url::Origin& origin,
    base::OnceCallback<void(base::Optional<std::vector<url::Origin>>,
                            base::Optional<std::string>,
                            base::Optional<content::SmsFetchFailureType>)>
        callback) {
  // TODO(crbug.com/1015645): We should have a new failure type when the feature
  // is disabled or no device is available.
  if (!base::FeatureList::IsEnabled(kWebOTPCrossDevice)) {
    std::move(callback).Run(base::nullopt, base::nullopt, base::nullopt);
    // kWebOTPCrossDevice will be disabled for a large number of users. There's
    // no need to call any cancel callback in such case.
    return base::NullCallback();
  }

// The current distinction of local fetcher being non-Android and remote fetcher
// being Android is a simplification we have made at this point and not a
// fundamental limitation. This may be relaxed in the future. e.g. allows
// tablets that run Android fetch a remote sms.
#if !defined(OS_ANDROID)
  auto* ui_controller =
      SmsRemoteFetcherUiController::GetOrCreateFromWebContents(web_contents);
  return ui_controller->FetchRemoteSms(origin, std::move(callback));
#else
  std::move(callback).Run(base::nullopt, base::nullopt, base::nullopt);
  return base::NullCallback();
#endif
}
