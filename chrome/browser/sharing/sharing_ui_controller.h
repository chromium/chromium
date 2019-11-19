// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARING_UI_CONTROLLER_H_
#define CHROME_BROWSER_SHARING_SHARING_UI_CONTROLLER_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/sharing/sharing_app.h"
#include "chrome/browser/sharing/sharing_constants.h"
#include "chrome/browser/sharing/sharing_dialog_data.h"
#include "chrome/browser/sharing/sharing_metrics.h"
#include "chrome/browser/sharing/sharing_service.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "components/sync/protocol/device_info_specifics.pb.h"
#include "components/sync/protocol/sharing_message.pb.h"
#include "components/sync_device_info/device_info.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/styled_label_listener.h"
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
  virtual base::string16 GetTitle(SharingDialogType dialog_type);
  // Called when user chooses a synced device to complete the task.
  virtual void OnDeviceChosen(const syncer::DeviceInfo& device) = 0;
  // Called when user chooses a local app to complete the task.
  virtual void OnAppChosen(const SharingApp& app) = 0;
  virtual PageActionIconType GetIconType() = 0;
  virtual sync_pb::SharingSpecificFields::EnabledFeatures
  GetRequiredFeature() = 0;
  virtual const gfx::VectorIcon& GetVectorIcon() const = 0;
  virtual base::string16 GetTextForTooltipAndAccessibleName() const = 0;
  // Get the name of the feature to be used as a prefix for the metric name.
  virtual SharingFeatureName GetFeatureMetricsPrefix() const = 0;
  // Describes the content type of shared data.
  virtual base::string16 GetContentType() const = 0;
  // Returns the message to be shown in the body of error dialog based on
  // |send_result_|.
  virtual base::string16 GetErrorDialogText() const;

  // Called by the SharingDialog when it is being closed.
  virtual void OnDialogClosed(SharingDialog* dialog);
  // Called by the SharingDialogView when the help text got clicked.
  virtual void OnHelpTextClicked(SharingDialogType dialog_type);
  // Called when a new dialog is shown.
  virtual void OnDialogShown(bool has_devices, bool has_apps);

  // Closes the current dialog and resets all state.
  void ClearLastDialog();

  // Gets the current list of apps and devices and shows a new dialog.
  void UpdateAndShowDialog(
      const base::Optional<url::Origin>& initiating_origin);

  // Gets the current list of devices that support the required feature.
  std::vector<std::unique_ptr<syncer::DeviceInfo>> GetDevices();

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

  void SendMessageToDevice(
      const syncer::DeviceInfo& device,
      chrome_browser_sharing::SharingMessage sharing_message);

 private:
  // Updates the omnibox icon if available.
  void UpdateIcon();
  // Closes the current dialog if there is one.
  void CloseDialog();
  // Shows a new SharingDialog and closes the old one.
  void ShowNewDialog(SharingDialogData dialog_data);

  base::string16 GetTargetDeviceName() const;

  // Called after a message got sent to a device. Shows a new error dialog if
  // |success| is false and updates the omnibox icon.
  void OnMessageSentToDevice(
      int dialog_id,
      SharingSendMessageResult result,
      std::unique_ptr<chrome_browser_sharing::ResponseMessage> response);

  void OnAppsReceived(int dialog_id,
                      const base::Optional<url::Origin>& initiating_origin,
                      std::vector<SharingApp> apps);

  SharingDialog* dialog_ = nullptr;
  content::WebContents* web_contents_ = nullptr;
  SharingService* sharing_service_ = nullptr;

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
