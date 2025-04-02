// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DATA_SHARING_DESKTOP_DATA_SHARING_SDK_DELEGATE_DESKTOP_H_
#define CHROME_BROWSER_DATA_SHARING_DESKTOP_DATA_SHARING_SDK_DELEGATE_DESKTOP_H_

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/webui/data_sharing/data_sharing_ui.h"
#include "components/data_sharing/public/data_sharing_sdk_delegate.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"

namespace data_sharing {

// Used by DataSharingService to provide access to SDK.
class DataSharingSDKDelegateDesktop : public DataSharingSDKDelegate,
                                      DataSharingUI::Delegate {
 public:
  using LoadFinishedCallback = base::OnceCallback<void(content::WebContents*)>;
  using ReadGroupsCallback = base::OnceCallback<void(
      const base::expected<data_sharing_pb::ReadGroupsResult, absl::Status>&)>;
  using ReadGroupWithTokenCallback = base::OnceCallback<void(
      const base::expected<data_sharing_pb::ReadGroupsResult, absl::Status>&)>;

  explicit DataSharingSDKDelegateDesktop(content::BrowserContext* context);

  DataSharingSDKDelegateDesktop(const DataSharingSDKDelegateDesktop&) = delete;
  DataSharingSDKDelegateDesktop& operator=(
      const DataSharingSDKDelegateDesktop&) = delete;
  DataSharingSDKDelegateDesktop(DataSharingSDKDelegateDesktop&&) = delete;
  DataSharingSDKDelegateDesktop& operator=(DataSharingSDKDelegateDesktop&&) =
      delete;

  ~DataSharingSDKDelegateDesktop() override;

  // DataSharingSDKDelegate:
  void Initialize(
      DataSharingNetworkLoader* data_sharing_network_loader) override;

  void CreateGroup(const data_sharing_pb::CreateGroupParams& params,
                   base::OnceCallback<void(
                       const base::expected<data_sharing_pb::CreateGroupResult,
                                            absl::Status>&)> callback) override;

  void ReadGroups(const data_sharing_pb::ReadGroupsParams& params,
                  ReadGroupsCallback callback) override;

  void ReadGroupWithToken(
      const data_sharing_pb::ReadGroupWithTokenParams& params,
      ReadGroupWithTokenCallback callback) override;

  void AddMember(
      const data_sharing_pb::AddMemberParams& params,
      base::OnceCallback<void(const absl::Status&)> callback) override;

  void RemoveMember(
      const data_sharing_pb::RemoveMemberParams& params,
      base::OnceCallback<void(const absl::Status&)> callback) override;

  void LeaveGroup(
      const data_sharing_pb::LeaveGroupParams& params,
      base::OnceCallback<void(const absl::Status&)> callback) override;

  void DeleteGroup(
      const data_sharing_pb::DeleteGroupParams& params,
      base::OnceCallback<void(const absl::Status&)> callback) override;

  void LookupGaiaIdByEmail(
      const data_sharing_pb::LookupGaiaIdByEmailParams& params,
      base::OnceCallback<
          void(const base::expected<data_sharing_pb::LookupGaiaIdByEmailResult,
                                    absl::Status>&)> callback) override;

  void Shutdown() override;

  // DataSharingUI::Delegate:
  void ApiInitComplete() override;
  void ShowErrorDialog(int status_code) override;
  void OnShareLinkRequested(
      const std::string& group_id,
      const std::string& access_token,
      base::OnceCallback<void(const std::optional<GURL>&)> callback) override;
  void OnGroupAction(
      data_sharing::mojom::GroupAction action,
      data_sharing::mojom::GroupActionProgress progress) override;

  void AddAccessToken(
      const data_sharing_pb::AddAccessTokenParams& params,
      base::OnceCallback<
          void(const base::expected<data_sharing_pb::AddAccessTokenResult,
                                    absl::Status>&)> callback) override;

 private:
  // Load the WebContents if it's not loaded and invoke the callback when
  // finish, otherwise invoke the callback immediately.
  void MaybeLoadWebContents(LoadFinishedCallback callback);

  void OnReadGroups(ReadGroupsCallback callback,
                    data_sharing::mojom::ReadGroupsResultPtr mojom_result);

  void OnReadGroupWithToken(
      ReadGroupWithTokenCallback callback,
      data_sharing::mojom::ReadGroupWithTokenResultPtr mojom_result);

  void OnLeaveGroup(base::OnceCallback<void(const absl::Status&)> callback,
                    int status_code);

  void OnDeleteGroup(base::OnceCallback<void(const absl::Status&)> callback,
                     int status_code);

  // Schedule reseting the WebContents after a specific time.
  void ScheduleResetWebContentsTimer();

  // Reset the WebContents.
  void ResetWebContents();

  // A list of callback to be invoked.
  base::OnceCallbackList<void(content::WebContents*)> callbacks_;

  // A list of callback subscriptions.
  std::vector<base::CallbackListSubscription> callback_subscriptions_;

  // WebContents to load the data sharing sdk.
  std::unique_ptr<content::WebContents> web_contents_;

  const raw_ptr<content::BrowserContext> context_;

  // Timer to reset the WebContents after some time without activity.
  std::unique_ptr<base::OneShotTimer> reset_web_contents_timer_;
};

}  // namespace data_sharing

#endif  // CHROME_BROWSER_DATA_SHARING_DESKTOP_DATA_SHARING_SDK_DELEGATE_DESKTOP_H_
