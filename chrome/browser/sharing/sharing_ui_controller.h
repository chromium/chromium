// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARING_UI_CONTROLLER_H_
#define CHROME_BROWSER_SHARING_SHARING_UI_CONTROLLER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "components/sharing_message/proto/sharing_message.pb.h"
#include "components/sharing_message/sharing_app.h"
#include "components/sharing_message/sharing_constants.h"
#include "components/sharing_message/sharing_dialog_data.h"
#include "components/sharing_message/sharing_metrics.h"
#include "components/sharing_message/sharing_service.h"
#include "components/sharing_message/sharing_target_device_info.h"
#include "components/sync/protocol/device_info_specifics.pb.h"
#include "url/origin.h"

class SharingDialog;
class SharingService;

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace content {
class WebContents;
}  // namespace content

// The controller for desktop dialog with the list of synced devices and apps.
class SharingUiController {
 public:
  using UpdateAppsCallback = base::OnceCallback<void(std::vector<SharingApp>)>;

  explicit SharingUiController(content::WebContents* web_contents);
  virtual ~SharingUiController();

  // Title of the dialog.
  virtual std::u16string GetTitle(SharingDialogType dialog_type);
  // Called when user chooses a synced device to complete the task.
  virtual void OnDeviceChosen(const SharingTargetDeviceInfo& device) = 0;
  // Called when user chooses a local app to complete the task.
  virtual void OnAppChosen(const SharingApp& app) = 0;
  virtual PageActionIconType GetIconType() = 0;
  virtual sync_pb::SharingSpecificFields::EnabledFeatures GetRequiredFeature()
      const = 0;
  virtual const gfx::VectorIcon& GetVectorIcon() const = 0;
  // If true, shows a loading icon on omnibox when sending out the message.
  virtual bool ShouldShowLoadingIcon() const;
  virtual std::u16string GetTextForTooltipAndAccessibleName() const = 0;

  // If false, any UI associated will be excluded from the accessibility tree,
  // making it completely undiscoverable and unusable to (at least) screen
  // reader users. If you override this function, please seek the review of
  // an accessibility OWNER and clearly document the use case in your code.
  virtual bool HasAccessibleUi() const;

  // Get the name of the feature to be used as a prefix for the metric name.
  virtual SharingFeatureName GetFeatureMetricsPrefix() const = 0;
  // Describes the content type of shared data. For most languages this
  // will be lower case as it's intended to be put as a placeholder within
  // a sentence.
  virtual std::u16string GetContentType() const = 0;
  // Returns the message to be shown in the body of error dialog based on
  // |send_result_|.
  virtual std::u16string GetErrorDialogText() const;

  // Called by the SharingDialog when it is being closed.
  virtual void OnDialogClosed(SharingDialog* dialog);
  // Called when a new dialog is shown.
  virtual void OnDialogShown(bool has_devices, bool has_apps);

  // Closes the current dialog and resets all state.
  void ClearLastDialog();

  // Gets the current list of apps and devices and shows a new dialog.
  void UpdateAndShowDialog(const std::optional<url::Origin>& initiating_origin);

  // Gets the current list of devices that support the required feature.
  std::vector<SharingTargetDeviceInfo> GetDevices() const;

  bool HasSendFailed() const;

  void MaybeShowErrorDialog();

  // Returns the currently open SharingDialog or nullptr if there is no
  // dialog open.
  SharingDialog* dialog() const { return dialog_; }
  bool is_loading() const { return is_loading_; }
  SharingSendMessageResult send_result() const { return send_result_; }
  void set_send_result_for_testing(SharingSendMessageResult result) {
    send_result_ = result;
  }
  content::WebContents* web_contents() const { return web_contents_; }

  void set_on_dialog_shown_closure_for_testing(base::OnceClosure closure) {
    on_dialog_shown_closure_for_testing_ = std::move(closure);
  }

 protected:
  virtual void DoUpdateApps(UpdateAppsCallback callback) = 0;
  // Prepares a new dialog data.
  virtual SharingDialogData CreateDialogData(SharingDialogType dialog_type);

  // Shows an icon in the omnibox which will be removed when receiving a
  // response or when cancelling the request by calling the returned callback.
  base::OnceClosure SendMessageToDevice(
      const SharingTargetDeviceInfo& device,
      std::optional<base::TimeDelta> response_timeout,
      components_sharing_message::SharingMessage sharing_message,
      std::optional<SharingMessageSender::ResponseCallback> callback);

  // Updates the omnibox icon if available.
  void UpdateIcon();

 private:
  // Closes the current dialog if there is one.
  void CloseDialog();
  // Shows a new SharingDialog and closes the old one.
  void ShowNewDialog(SharingDialogData dialog_data);

  std::u16string GetTargetDeviceName() const;

  // Called after a message got sent to a device. Shows a new error dialog if
  // |success| is false and updates the omnibox icon. The client can handle the
  // response via |custom_callback|.
  void OnResponse(
      int dialog_id,
      std::optional<SharingMessageSender::ResponseCallback> custom_callback,
      SharingSendMessageResult result,
      std::unique_ptr<components_sharing_message::ResponseMessage> response);

  void OnAppsReceived(int dialog_id,
                      const std::optional<url::Origin>& initiating_origin,
                      std::vector<SharingApp> apps);

  raw_ptr<SharingDialog> dialog_ = nullptr;
  raw_ptr<content::WebContents> web_contents_ = nullptr;
  raw_ptr<SharingService> sharing_service_ = nullptr;

  bool is_loading_ = false;
  SharingSendMessageResult send_result_ = SharingSendMessageResult::kSuccessful;
  std::string target_device_name_;

  // ID of the last shown dialog used to ignore events from old dialogs.
  int last_dialog_id_ = 0;

  // Closure to call when a new dialog is shown.
  base::OnceClosure on_dialog_shown_closure_for_testing_;

  base::WeakPtrFactory<SharingUiController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SHARING_SHARING_UI_CONTROLLER_H_
