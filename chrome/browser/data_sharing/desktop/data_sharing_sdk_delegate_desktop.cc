// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/data_sharing/desktop/data_sharing_sdk_delegate_desktop.h"

#include "base/notimplemented.h"
#include "chrome/browser/data_sharing/desktop/data_sharing_conversion_utils.h"
#include "chrome/browser/ui/webui/data_sharing/data_sharing_page_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/navigation_controller.h"

namespace data_sharing {

namespace {
constexpr base::TimeDelta kResetWebContentsAfterLastCallDuration =
    base::Minutes(1);
}

DataSharingSDKDelegateDesktop::DataSharingSDKDelegateDesktop(
    content::BrowserContext* context)
    : context_(context),
      reset_web_contents_timer_(std::make_unique<base::OneShotTimer>()) {}

DataSharingSDKDelegateDesktop::~DataSharingSDKDelegateDesktop() = default;

void DataSharingSDKDelegateDesktop::Initialize(
    DataSharingNetworkLoader* data_sharing_network_loader) {}

void DataSharingSDKDelegateDesktop::CreateGroup(
    const data_sharing_pb::CreateGroupParams& params,
    base::OnceCallback<
        void(const base::expected<data_sharing_pb::CreateGroupResult,
                                  absl::Status>&)> callback) {
  NOTIMPLEMENTED();
}

void DataSharingSDKDelegateDesktop::ReadGroups(
    const data_sharing_pb::ReadGroupsParams& params,
    ReadGroupsCallback callback) {
  MaybeLoadWebContents(base::BindOnce(
      [](data_sharing_pb::ReadGroupsParams params, ReadGroupsCallback callback,
         DataSharingSDKDelegateDesktop* delegate,
         content::WebContents* web_contents) {
        DataSharingPageHandler* handler =
            static_cast<DataSharingUI*>(
                web_contents->GetWebUI()->GetController())
                ->page_handler();
        CHECK(handler);
        auto mojom_params = data_sharing::mojom::ReadGroupsParams::New();
        for (const auto& group_param : params.group_params()) {
          auto param = data_sharing::mojom::ReadGroupParams::New();
          param->group_id = group_param.group_id();
          param->consistency_token = group_param.consistency_token();
          mojom_params->params.push_back(std::move(param));
        }
        handler->ReadGroups(
            std::move(mojom_params),
            base::BindOnce(&DataSharingSDKDelegateDesktop::OnReadGroups,
                           base::Unretained(delegate), std::move(callback)));
      },
      params, std::move(callback), this));
}

void DataSharingSDKDelegateDesktop::AddMember(
    const data_sharing_pb::AddMemberParams& params,
    base::OnceCallback<void(const absl::Status&)> callback) {
  NOTIMPLEMENTED();
}

void DataSharingSDKDelegateDesktop::RemoveMember(
    const data_sharing_pb::RemoveMemberParams& params,
    base::OnceCallback<void(const absl::Status&)> callback) {
  NOTIMPLEMENTED();
}

void DataSharingSDKDelegateDesktop::LeaveGroup(
    const data_sharing_pb::LeaveGroupParams& params,
    base::OnceCallback<void(const absl::Status&)> callback) {
  MaybeLoadWebContents(base::BindOnce(
      [](data_sharing_pb::LeaveGroupParams params,
         base::OnceCallback<void(const absl::Status&)> callback,
         DataSharingSDKDelegateDesktop* delegate,
         content::WebContents* web_contents) {
        DataSharingPageHandler* handler =
            static_cast<DataSharingUI*>(
                web_contents->GetWebUI()->GetController())
                ->page_handler();
        CHECK(handler);
        handler->LeaveGroup(
            params.group_id(),
            base::BindOnce(&DataSharingSDKDelegateDesktop::OnLeaveGroup,
                           base::Unretained(delegate), std::move(callback)));
      },
      params, std::move(callback), this));
}

void DataSharingSDKDelegateDesktop::DeleteGroup(
    const data_sharing_pb::DeleteGroupParams& params,
    base::OnceCallback<void(const absl::Status&)> callback) {
  MaybeLoadWebContents(base::BindOnce(
      [](data_sharing_pb::DeleteGroupParams params,
         base::OnceCallback<void(const absl::Status&)> callback,
         DataSharingSDKDelegateDesktop* delegate,
         content::WebContents* web_contents) {
        DataSharingPageHandler* handler =
            static_cast<DataSharingUI*>(
                web_contents->GetWebUI()->GetController())
                ->page_handler();
        CHECK(handler);
        handler->DeleteGroup(
            params.group_id(),
            base::BindOnce(&DataSharingSDKDelegateDesktop::OnDeleteGroup,
                           base::Unretained(delegate), std::move(callback)));
      },
      params, std::move(callback), this));
}

void DataSharingSDKDelegateDesktop::LookupGaiaIdByEmail(
    const data_sharing_pb::LookupGaiaIdByEmailParams& params,
    base::OnceCallback<
        void(const base::expected<data_sharing_pb::LookupGaiaIdByEmailResult,
                                  absl::Status>&)> callback) {
  NOTIMPLEMENTED();
}

void DataSharingSDKDelegateDesktop::AddAccessToken(
    const data_sharing_pb::AddAccessTokenParams& params,
    base::OnceCallback<
        void(const base::expected<data_sharing_pb::AddAccessTokenResult,
                                  absl::Status>&)> callback) {
  NOTIMPLEMENTED();
}

void DataSharingSDKDelegateDesktop::MaybeLoadWebContents(
    LoadFinishedCallback callback) {
  // Load the WebContents if it's not loaded yet.
  if (!web_contents_) {
    GURL webui_url(chrome::kChromeUIUntrustedDataSharingAPIURL);
    content::WebContents::CreateParams create_params(context_);
    create_params.initially_hidden = true;
    create_params.site_instance =
        content::SiteInstance::CreateForURL(context_, webui_url);
    web_contents_ = content::WebContents::Create(create_params);

    web_contents_->GetController().LoadURL(webui_url, content::Referrer(),
                                           ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                           /*extra_headers=*/std::string());
  }

  // If the API is already initialized, run the callback here, otherwise add the
  // callback to the queue and run it when `ApiInitComplete` is called.
  DataSharingUI* data_sharing_ui =
      static_cast<DataSharingUI*>(web_contents_->GetWebUI()->GetController());
  if (data_sharing_ui->IsApiInitialized()) {
    std::move(callback).Run(web_contents_.get());
  } else {
    data_sharing_ui->SetDelegate(this);
    callback_subscriptions_.emplace_back(callbacks_.Add(std::move(callback)));
  }

  // For every API call, schedule a timer to clean up the WebContents after some
  // time if there are no more calls coming after.
  ScheduleResetWebContentsTimer();
}

void DataSharingSDKDelegateDesktop::ApiInitComplete() {
  // At this point the page handler should be created.
  // Invoke the callbacks and clear the subscriptions.
  DataSharingUI* data_sharing_ui =
      static_cast<DataSharingUI*>(web_contents_->GetWebUI()->GetController());
  data_sharing_ui->SetDelegate(nullptr);
  CHECK(data_sharing_ui->page_handler());
  callbacks_.Notify(web_contents_.get());
  callback_subscriptions_.clear();
}

void DataSharingSDKDelegateDesktop::ShowErrorDialog(int status_code) {
  // No-op for this class.
}

void DataSharingSDKDelegateDesktop::OnShareLinkRequested(
    const std::string& group_id,
    const std::string& access_token,
    base::OnceCallback<void(const std::optional<GURL>&)> callback) {
  // No-op for this class.
}

void DataSharingSDKDelegateDesktop::OnGroupAction(
    data_sharing::mojom::GroupAction action,
    data_sharing::mojom::GroupActionProgress progress) {
  // No-op for this class.
}

void DataSharingSDKDelegateDesktop::Shutdown() {
  // Since WebContents depends on BrowserContext, it needs to be destroyed
  // before the BrowserContext is destroyed.
  ResetWebContents();
}

void DataSharingSDKDelegateDesktop::OnReadGroups(
    ReadGroupsCallback callback,
    data_sharing::mojom::ReadGroupsResultPtr mojom_result) {
  if (mojom_result->status_code != 0) {
    std::move(callback).Run(base::unexpected(
        absl::Status(static_cast<absl::StatusCode>(mojom_result->status_code),
                     "Read Groups failed")));
    return;
  }
  data_sharing_pb::ReadGroupsResult result;
  for (auto& group : mojom_result->groups) {
    *result.add_group_data() = ConvertGroup(group);
  }
  std::move(callback).Run(result);
}

void DataSharingSDKDelegateDesktop::OnReadGroupWithToken(
    ReadGroupWithTokenCallback callback,
    data_sharing::mojom::ReadGroupWithTokenResultPtr mojom_result) {
  if (mojom_result->status_code != 0) {
    std::move(callback).Run(base::unexpected(
        absl::Status(static_cast<absl::StatusCode>(mojom_result->status_code),
                     "Read Groups with token failed")));
    return;
  }
  data_sharing_pb::ReadGroupsResult result;
  *result.add_group_data() = ConvertGroup(mojom_result->group);
  std::move(callback).Run(result);
}

void DataSharingSDKDelegateDesktop::ReadGroupWithToken(
    const data_sharing_pb::ReadGroupWithTokenParams& params,
    base::OnceCallback<void(
        const base::expected<data_sharing_pb::ReadGroupsResult, absl::Status>&)>
        callback) {
  MaybeLoadWebContents(base::BindOnce(
      [](data_sharing_pb::ReadGroupWithTokenParams params,
         ReadGroupWithTokenCallback callback,
         DataSharingSDKDelegateDesktop* delegate,
         content::WebContents* web_contents) {
        DataSharingPageHandler* handler =
            static_cast<DataSharingUI*>(
                web_contents->GetWebUI()->GetController())
                ->page_handler();
        CHECK(handler);
        auto mojom_param = data_sharing::mojom::ReadGroupWithTokenParam::New();
        mojom_param->group_id = params.group_id();
        mojom_param->access_token = params.access_token();
        handler->ReadGroupWithToken(
            std::move(mojom_param),
            base::BindOnce(&DataSharingSDKDelegateDesktop::OnReadGroupWithToken,
                           base::Unretained(delegate), std::move(callback)));
      },
      params, std::move(callback), this));
}

void DataSharingSDKDelegateDesktop::OnLeaveGroup(
    base::OnceCallback<void(const absl::Status&)> callback,
    int status_code) {
  std::move(callback).Run(
      absl::Status(static_cast<absl::StatusCode>(status_code), "Leave Group"));
}

void DataSharingSDKDelegateDesktop::OnDeleteGroup(
    base::OnceCallback<void(const absl::Status&)> callback,
    int status_code) {
  std::move(callback).Run(
      absl::Status(static_cast<absl::StatusCode>(status_code), "Delete Group"));
}

void DataSharingSDKDelegateDesktop::ScheduleResetWebContentsTimer() {
  reset_web_contents_timer_->Start(
      FROM_HERE, kResetWebContentsAfterLastCallDuration, this,
      &DataSharingSDKDelegateDesktop::ResetWebContents);
}

void DataSharingSDKDelegateDesktop::ResetWebContents() {
  callback_subscriptions_.clear();
  web_contents_.reset();
}

}  // namespace data_sharing
