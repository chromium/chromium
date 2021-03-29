// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sms/sms_remote_fetcher_ui_controller.h"

#include <utility>

#include "base/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/sharing/sharing_constants.h"
#include "chrome/browser/sharing/sharing_dialog.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/grit/generated_resources.h"
#include "components/sync_device_info/device_info.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/sms_fetcher.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/sms/webotp_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/strings/grit/ui_strings.h"

using SharingMessage = chrome_browser_sharing::SharingMessage;

// static
SmsRemoteFetcherUiController*
SmsRemoteFetcherUiController::GetOrCreateFromWebContents(
    content::WebContents* web_contents) {
  SmsRemoteFetcherUiController::CreateForWebContents(web_contents);
  return SmsRemoteFetcherUiController::FromWebContents(web_contents);
}

SmsRemoteFetcherUiController::SmsRemoteFetcherUiController(
    content::WebContents* web_contents)
    : SharingUiController(web_contents) {}

SmsRemoteFetcherUiController::~SmsRemoteFetcherUiController() = default;

PageActionIconType SmsRemoteFetcherUiController::GetIconType() {
  return PageActionIconType::kSmsRemoteFetcher;
}

sync_pb::SharingSpecificFields::EnabledFeatures
SmsRemoteFetcherUiController::GetRequiredFeature() const {
  return sync_pb::SharingSpecificFields::SMS_FETCHER;
}

void SmsRemoteFetcherUiController::DoUpdateApps(UpdateAppsCallback callback) {
  std::move(callback).Run(std::vector<SharingApp>());
}

void SmsRemoteFetcherUiController::OnDeviceChosen(
    const syncer::DeviceInfo& device) {}

void SmsRemoteFetcherUiController::OnAppChosen(const SharingApp& app) {}

std::u16string SmsRemoteFetcherUiController::GetContentType() const {
  return l10n_util::GetStringUTF16(IDS_BROWSER_SHARING_CONTENT_TYPE_TEXT);
}

const gfx::VectorIcon& SmsRemoteFetcherUiController::GetVectorIcon() const {
  return kSmartphoneIcon;
}

std::u16string
SmsRemoteFetcherUiController::GetTextForTooltipAndAccessibleName() const {
  return l10n_util::GetStringFUTF16(IDS_OMNIBOX_TOOLTIP_SMS_REMOTE_FETCHER,
                                    base::UTF8ToUTF16(last_device_name_));
}

SharingFeatureName SmsRemoteFetcherUiController::GetFeatureMetricsPrefix()
    const {
  return SharingFeatureName::kSmsRemoteFetcher;
}

void SmsRemoteFetcherUiController::OnSmsRemoteFetchResponse(
    OnRemoteCallback callback,
    SharingSendMessageResult result,
    std::unique_ptr<chrome_browser_sharing::ResponseMessage> response) {
  if (result != SharingSendMessageResult::kSuccessful) {
    // TODO(crbug.com/1015645): We should have a new category for remote
    // failures.
    std::move(callback).Run(base::nullopt, base::nullopt, base::nullopt);
    return;
  }

  DCHECK(response);
  DCHECK(response->has_sms_fetch_response());
  if (response->sms_fetch_response().has_failure_type()) {
    std::move(callback).Run(base::nullopt, base::nullopt,
                            static_cast<content::SmsFetchFailureType>(
                                response->sms_fetch_response().failure_type()));
    return;
  }
  auto origin_strings = response->sms_fetch_response().origin();
  std::vector<url::Origin> origin_list;
  for (const std::string& origin_string : origin_strings)
    origin_list.push_back(url::Origin::Create(GURL(origin_string)));

  std::move(callback).Run(std::move(origin_list),
                          response->sms_fetch_response().one_time_code(),
                          base::nullopt);
}

base::OnceClosure SmsRemoteFetcherUiController::FetchRemoteSms(
    const url::Origin& origin,
    OnRemoteCallback callback) {
  SharingService::SharingDeviceList devices = GetDevices();

  if (devices.empty()) {
    // No devices available to call.
    // TODO(crbug.com/1015645): We should have a new category for remote
    // failures.
    std::move(callback).Run(base::nullopt, base::nullopt, base::nullopt);
    return base::NullCallback();
  }

  // Sends to the first device that has the capability enabled. User cannot
  // select device because the site sends out the SMS asynchronously.
  const std::unique_ptr<syncer::DeviceInfo>& device = devices.front();
  last_device_name_ = device->client_name();
  chrome_browser_sharing::SharingMessage request;

  request.mutable_sms_fetch_request()->set_origin(origin.Serialize());

  return SendMessageToDevice(
      *device.get(), blink::kWebOTPRequestTimeout, std::move(request),
      base::BindOnce(&SmsRemoteFetcherUiController::OnSmsRemoteFetchResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SmsRemoteFetcherUiController)
