// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sms/sms_remote_fetcher_ui_controller.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/sharing/sms/sms_remote_fetcher_metrics.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/grit/generated_resources.h"
#include "components/sharing_message/sharing_constants.h"
#include "components/sharing_message/sharing_dialog.h"
#include "components/sharing_message/sharing_target_device_info.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/sms_fetcher.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/sms/webotp_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/strings/grit/ui_strings.h"

using SharingMessage = components_sharing_message::SharingMessage;

// static
SmsRemoteFetcherUiController*
SmsRemoteFetcherUiController::GetOrCreateFromWebContents(
    content::WebContents* web_contents) {
  SmsRemoteFetcherUiController::CreateForWebContents(web_contents);
  return SmsRemoteFetcherUiController::FromWebContents(web_contents);
}

SmsRemoteFetcherUiController::SmsRemoteFetcherUiController(
    content::WebContents* web_contents)
    : SharingUiController(web_contents),
      content::WebContentsUserData<SmsRemoteFetcherUiController>(
          *web_contents) {}

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
    const SharingTargetDeviceInfo& device) {}

void SmsRemoteFetcherUiController::OnAppChosen(const SharingApp& app) {}

std::u16string SmsRemoteFetcherUiController::GetContentType() const {
  return l10n_util::GetStringUTF16(IDS_BROWSER_SHARING_CONTENT_TYPE_TEXT);
}

const gfx::VectorIcon& SmsRemoteFetcherUiController::GetVectorIcon() const {
  return kSmartphoneRefreshIcon;
}

bool SmsRemoteFetcherUiController::ShouldShowLoadingIcon() const {
  return false;
}

std::u16string
SmsRemoteFetcherUiController::GetTextForTooltipAndAccessibleName() const {
  return std::u16string();
}

bool SmsRemoteFetcherUiController::HasAccessibleUi() const {
  // crrev.com/c/2964059 stopped all UI from being shown and removed the
  // accessible name. That did not remove the icon from the accessibility
  // tree. To stop this UI from being shown to assistive technologies, we
  // return false here.
  return false;
}

SharingFeatureName SmsRemoteFetcherUiController::GetFeatureMetricsPrefix()
    const {
  return SharingFeatureName::kSmsRemoteFetcher;
}

void SmsRemoteFetcherUiController::OnSmsRemoteFetchResponse(
    OnRemoteCallback callback,
    SharingSendMessageResult result,
    std::unique_ptr<components_sharing_message::ResponseMessage> response) {
  if (result != SharingSendMessageResult::kSuccessful) {
    std::move(callback).Run(std::nullopt, std::nullopt,
                            content::SmsFetchFailureType::kCrossDeviceFailure);
    RecordWebOTPCrossDeviceFailure(
        WebOTPCrossDeviceFailure::kSharingMessageFailure);
    return;
  }

  DCHECK(response);
  DCHECK(response->has_sms_fetch_response());
  if (response->sms_fetch_response().has_failure_type()) {
    std::move(callback).Run(std::nullopt, std::nullopt,
                            static_cast<content::SmsFetchFailureType>(
                                response->sms_fetch_response().failure_type()));
    RecordWebOTPCrossDeviceFailure(
        WebOTPCrossDeviceFailure::kAPIFailureOnAndroid);
    return;
  }
  auto origin_strings = response->sms_fetch_response().origins();
  std::vector<url::Origin> origin_list;
  for (const std::string& origin_string : origin_strings)
    origin_list.push_back(url::Origin::Create(GURL(origin_string)));

  std::move(callback).Run(std::move(origin_list),
                          response->sms_fetch_response().one_time_code(),
                          std::nullopt);
  RecordWebOTPCrossDeviceFailure(WebOTPCrossDeviceFailure::kNoFailure);
}

base::OnceClosure SmsRemoteFetcherUiController::FetchRemoteSms(
    const std::vector<url::Origin>& origin_list,
    OnRemoteCallback callback) {
  SharingService::SharingDeviceList devices = GetDevices();
  base::UmaHistogramExactLinear("Sharing.SmsFetcherAvailableDeviceCount",
                                devices.size(),
                                /*value_max=*/20);

  if (devices.empty()) {
    std::move(callback).Run(std::nullopt, std::nullopt,
                            content::SmsFetchFailureType::kCrossDeviceFailure);
    RecordWebOTPCrossDeviceFailure(WebOTPCrossDeviceFailure::kNoRemoteDevice);
    return base::NullCallback();
  }

  // Sends to the first device that has the capability enabled. User cannot
  // select device because the site sends out the SMS asynchronously.
  const SharingTargetDeviceInfo& device = devices.front();
  last_device_name_ = device.client_name();
  components_sharing_message::SharingMessage request;

  for (const url::Origin& origin : origin_list)
    request.mutable_sms_fetch_request()->add_origins(origin.Serialize());

  return SendMessageToDevice(
      device, blink::kWebOTPRequestTimeout, std::move(request),
      base::BindOnce(&SmsRemoteFetcherUiController::OnSmsRemoteFetchResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SmsRemoteFetcherUiController);
