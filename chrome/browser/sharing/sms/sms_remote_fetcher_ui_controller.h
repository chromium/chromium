// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SMS_SMS_REMOTE_FETCHER_UI_CONTROLLER_H_
#define CHROME_BROWSER_SHARING_SMS_SMS_REMOTE_FETCHER_UI_CONTROLLER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/sharing/sharing_ui_controller.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "components/sharing_message/sharing_metrics.h"
#include "components/sharing_message/sharing_service.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
class WebContents;
enum class SmsFetchFailureType;
}  // namespace content

// Manages remote sms fetching related communication between desktop and Android
// and the desktop UI around it.
class SmsRemoteFetcherUiController
    : public SharingUiController,
      public content::WebContentsUserData<SmsRemoteFetcherUiController> {
 public:
  using OnRemoteCallback =
      base::OnceCallback<void(std::optional<std::vector<url::Origin>>,
                              std::optional<std::string>,
                              std::optional<content::SmsFetchFailureType>)>;
  static SmsRemoteFetcherUiController* GetOrCreateFromWebContents(
      content::WebContents* web_contents);

  SmsRemoteFetcherUiController(const SmsRemoteFetcherUiController&) = delete;
  SmsRemoteFetcherUiController& operator=(const SmsRemoteFetcherUiController&) =
      delete;
  ~SmsRemoteFetcherUiController() override;

  // Overridden from SharingUiController:
  PageActionIconType GetIconType() override;
  sync_pb::SharingSpecificFields::EnabledFeatures GetRequiredFeature()
      const override;
  void OnDeviceChosen(const SharingTargetDeviceInfo& device) override;
  void OnAppChosen(const SharingApp& app) override;
  std::u16string GetContentType() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  bool ShouldShowLoadingIcon() const override;
  std::u16string GetTextForTooltipAndAccessibleName() const override;
  SharingFeatureName GetFeatureMetricsPrefix() const override;
  bool HasAccessibleUi() const override;

  void OnSmsRemoteFetchResponse(
      OnRemoteCallback callback,
      SharingSendMessageResult result,
      std::unique_ptr<components_sharing_message::ResponseMessage> response);

  base::OnceClosure FetchRemoteSms(const std::vector<url::Origin>& origin_list,
                                   OnRemoteCallback callback);

 protected:
  explicit SmsRemoteFetcherUiController(content::WebContents* web_contents);

  // Overridden from SharingUiController:
  void DoUpdateApps(UpdateAppsCallback callback) override;

 private:
  friend class content::WebContentsUserData<SmsRemoteFetcherUiController>;

  std::string last_device_name_;

  base::WeakPtrFactory<SmsRemoteFetcherUiController> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_SHARING_SMS_SMS_REMOTE_FETCHER_UI_CONTROLLER_H_
