// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sms/sms_remote_fetcher.h"

#include "base/check.h"
#include "chrome/browser/sharing/sharing_constants.h"
#include "chrome/browser/sharing/sharing_service.h"
#include "chrome/browser/sharing/sharing_service_factory.h"
#include "chrome/browser/sharing/sms/sms_flags.h"
#include "components/sync_device_info/device_info.h"
#include "content/public/browser/browser_context.h"
#include "url/origin.h"

namespace {
const uint32_t kDefaultTimeoutSeconds = 60;
}  // namespace

void FetchRemoteSms(
    content::BrowserContext* context,
    const url::Origin& origin,
    base::OnceCallback<void(base::Optional<std::string>)> callback) {
  if (!base::FeatureList::IsEnabled(kWebOTPCrossDevice)) {
    std::move(callback).Run(base::nullopt);
    return;
  }

  SharingService* sharing_service =
      SharingServiceFactory::GetForBrowserContext(context);
  SharingService::SharingDeviceList devices =
      sharing_service->GetDeviceCandidates(
          sync_pb::SharingSpecificFields::SMS_FETCHER);

  if (devices.empty()) {
    // No devices available to call.
    std::move(callback).Run(base::nullopt);
    return;
  }

  // Sends to the first device that has the capability enabled.
  // TODO(crbug.com/1015645): figure out the routing strategy, possibly
  // requiring UX to allow the users to specify the device.
  const std::unique_ptr<syncer::DeviceInfo>& device = devices.front();

  chrome_browser_sharing::SharingMessage request;

  request.mutable_sms_fetch_request()->set_origin(origin.Serialize());

  sharing_service->SendMessageToDevice(
      *device.get(), base::TimeDelta::FromSeconds(kDefaultTimeoutSeconds),
      std::move(request),
      base::BindOnce(
          [](base::OnceCallback<void(base::Optional<std::string>)> callback,
             SharingSendMessageResult result,
             std::unique_ptr<chrome_browser_sharing::ResponseMessage>
                 response) {
            if (result != SharingSendMessageResult::kSuccessful) {
              std::move(callback).Run(base::nullopt);
              return;
            }

            DCHECK(response);
            DCHECK(response->has_sms_fetch_response());

            std::move(callback).Run(
                response->sms_fetch_response().one_time_code());
          },
          std::move(callback)));
}
