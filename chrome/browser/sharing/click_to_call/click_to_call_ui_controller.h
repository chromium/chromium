// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_UI_CONTROLLER_H_
#define CHROME_BROWSER_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_UI_CONTROLLER_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/sharing/click_to_call/click_to_call_metrics.h"
#include "chrome/browser/sharing/sharing_service.h"
#include "chrome/browser/sharing/sharing_ui_controller.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
class WebContents;
}  // namespace content

class ClickToCallUiController
    : public SharingUiController,
      public content::WebContentsUserData<ClickToCallUiController> {
 public:
  static ClickToCallUiController* GetOrCreateFromWebContents(
      content::WebContents* web_contents);
  static void ShowDialog(content::WebContents* web_contents,
                         const base::Optional<url::Origin>& initiating_origin,
                         const GURL& url,
                         bool hide_default_handler);

  ~ClickToCallUiController() override;

  void OnDeviceSelected(const std::string& phone_number,
                        const syncer::DeviceInfo& device,
                        SharingClickToCallEntryPoint entry_point);

  // Overridden from SharingUiController:
  std::u16string GetTitle(SharingDialogType dialog_type) override;
  PageActionIconType GetIconType() override;
  sync_pb::SharingSpecificFields::EnabledFeatures GetRequiredFeature()
      const override;
  void OnDeviceChosen(const syncer::DeviceInfo& device) override;
  void OnAppChosen(const SharingApp& app) override;
  void OnDialogClosed(SharingDialog* dialog) override;
  std::u16string GetContentType() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  std::u16string GetTextForTooltipAndAccessibleName() const override;
  SharingFeatureName GetFeatureMetricsPrefix() const override;
  void OnDialogShown(bool has_devices, bool has_apps) override;

 protected:
  explicit ClickToCallUiController(content::WebContents* web_contents);

  // Overridden from SharingUiController:
  void DoUpdateApps(UpdateAppsCallback callback) override;
  SharingDialogData CreateDialogData(SharingDialogType dialog_type) override;

 private:
  friend class content::WebContentsUserData<ClickToCallUiController>;
  using UKMRecorderCallback =
      base::OnceCallback<void(SharingClickToCallSelection)>;

  // Sends |phone_number| to |device| as a SharingMessage.
  void SendNumberToDevice(const syncer::DeviceInfo& device,
                          const std::string& phone_number,
                          SharingClickToCallEntryPoint entry_point);

  UKMRecorderCallback ukm_recorder_;
  GURL phone_url_;
  bool hide_default_handler_ = false;

  base::WeakPtrFactory<ClickToCallUiController> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(ClickToCallUiController);
};

#endif  // CHROME_BROWSER_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_UI_CONTROLLER_H_
