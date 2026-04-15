// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/send_tab_to_self/send_tab_to_self_page_handler.h"

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/metrics_util.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/sessions/content/content_serialized_navigation_builder.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "components/shared_highlighting/core/common/text_fragment.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace send_tab_to_self {

SendTabToSelfPageHandler::~SendTabToSelfPageHandler() = default;

// static
SendTabToSelfPageHandler* SendTabToSelfPageHandler::GetOrCreateForWebContents(
    content::WebContents* web_contents) {
  SendTabToSelfPageHandler::CreateForWebContents(web_contents);
  return SendTabToSelfPageHandler::FromWebContents(web_contents);
}

void SendTabToSelfPageHandler::SendTabToDevice(
    const std::string& target_device_guid,
    const GURL& url,
    const std::string& title,
    base::OnceCallback<void(SendTabToSelfResult)> commit_confirmation) {
  PendingRequest request(target_device_guid, url, title,
                         std::move(commit_confirmation));

  MaybeExtractFormFields(request);
  MaybeExtractNavigationHistory(request);

  // If the URL has changed or the scroll position feature is not enabled, send
  // the request without the scroll position information.
  if (request.url != web_contents()->GetLastCommittedURL() ||
      !base::FeatureList::IsEnabled(kSendTabToSelfPropagateScrollPosition)) {
    SendFinalizedRequest(std::move(request), std::nullopt);
    return;
  }

  // Otherwise, generate a token for the request and request the scroll
  // position selector from the renderer.
  base::Token request_token = base::Token::CreateRandom();
  RequestScrollPositionSelectorAndSendRequest(request_token,
                                              std::move(request));
}

void SendTabToSelfPageHandler::SetSelectorGenerationTimeoutForTesting(
    base::TimeDelta timeout) {
  selector_generation_timeout_for_testing_ = timeout;
}

SendTabToSelfPageHandler::SendTabToSelfPageHandler(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<SendTabToSelfPageHandler>(*web_contents) {}

void SendTabToSelfPageHandler::PrimaryPageChanged(content::Page& /*page*/) {
  CancelPendingRequests(ScrollPositionGenerationOutcome::kMainFrameChanged);
}

void SendTabToSelfPageHandler::WebContentsDestroyed() {
  CancelPendingRequests(ScrollPositionGenerationOutcome::kMainFrameUnavailable);
}

void SendTabToSelfPageHandler::CancelPendingRequests(
    ScrollPositionGenerationOutcome outcome) {
  base::flat_map<base::Token, PendingRequest> requests =
      std::move(pending_requests_);
  pending_requests_.clear();
  for (std::pair<base::Token, PendingRequest>& pair : requests) {
    SendFinalizedRequest(std::move(pair.second), outcome);
  }
}

SendTabToSelfPageHandler::PendingRequest::PendingRequest(
    const std::string& target_device_guid,
    const GURL& url,
    const std::string& title,
    base::OnceCallback<void(SendTabToSelfResult)> commit_confirmation)
    : target_device_guid(target_device_guid),
      url(url),
      title(title),
      start_time(base::TimeTicks::Now()),
      commit_confirmation(std::move(commit_confirmation)) {}

SendTabToSelfPageHandler::PendingRequest::PendingRequest(PendingRequest&&) =
    default;

SendTabToSelfPageHandler::PendingRequest&
SendTabToSelfPageHandler::PendingRequest::operator=(PendingRequest&&) = default;

SendTabToSelfPageHandler::PendingRequest::~PendingRequest() = default;

std::optional<SendTabToSelfPageHandler::PendingRequest>
SendTabToSelfPageHandler::TakePendingRequest(base::Token request_token) {
  auto it = pending_requests_.find(request_token);
  if (it == pending_requests_.end()) {
    return std::nullopt;
  }

  PendingRequest request = std::move(it->second);
  pending_requests_.erase(it);
  return request;
}

void SendTabToSelfPageHandler::SelectorGeneratedForRequest(
    base::Token request_token,
    const std::string& selector,
    shared_highlighting::LinkGenerationError error,
    shared_highlighting::LinkGenerationReadyStatus /*ready_status*/) {
  std::optional<PendingRequest> request = TakePendingRequest(request_token);
  if (!request) {
    return;
  }

  std::pair<ScrollPositionGenerationOutcome, ScrollPosition> result =
      ProcessSelectorGenerationResult(*request, selector, error);
  request->page_context.scroll_position = std::move(result.second);

  SendFinalizedRequest(std::move(*request), result.first);
}

void SendTabToSelfPageHandler::SelectorGenerationTimedOutForRequest(
    base::Token request_token) {
  std::optional<PendingRequest> request = TakePendingRequest(request_token);
  if (!request) {
    return;
  }

  content::RenderFrameHost* main_frame = web_contents()->GetPrimaryMainFrame();
  ScrollPositionGenerationOutcome outcome =
      (!main_frame || main_frame->GetGlobalId() != request->main_frame_id)
          ? ScrollPositionGenerationOutcome::kMainFrameChanged
          : ScrollPositionGenerationOutcome::kBrowserTimeout;

  SendFinalizedRequest(std::move(*request), outcome);
}

void SendTabToSelfPageHandler::RequestScrollPositionSelectorAndSendRequest(
    base::Token request_token,
    PendingRequest request) {
  content::RenderFrameHost* main_frame = web_contents()->GetPrimaryMainFrame();
  if (!main_frame) {
    SendFinalizedRequest(
        std::move(request),
        ScrollPositionGenerationOutcome::kMainFrameUnavailable);
    return;
  }

  request.main_frame_id = main_frame->GetGlobalId();

  if (request.main_frame_id != last_main_frame_id_) {
    text_fragment_receiver_.reset();
    last_main_frame_id_ = request.main_frame_id;
  }

  pending_requests_.insert_or_assign(request_token, std::move(request));

  if (!text_fragment_receiver_.is_bound()) {
    main_frame->GetRemoteInterfaces()->GetInterface(
        text_fragment_receiver_.BindNewPipeAndPassReceiver());
  }

  text_fragment_receiver_->RequestSelectorForViewportCenter(
      base::BindOnce(&SendTabToSelfPageHandler::SelectorGeneratedForRequest,
                     weak_ptr_factory_.GetWeakPtr(), request_token));

  // A 200ms timeout is implemented to avoid delaying the sharing process. If
  // the selector generation takes longer, or if the page is navigated or
  // destroyed, the tab is sent without the scroll position information.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &SendTabToSelfPageHandler::SelectorGenerationTimedOutForRequest,
          weak_ptr_factory_.GetWeakPtr(), request_token),
      GetSelectorGenerationTimeout());
}

std::pair<ScrollPositionGenerationOutcome, ScrollPosition>
SendTabToSelfPageHandler::ProcessSelectorGenerationResult(
    const PendingRequest& request,
    const std::string& selector,
    shared_highlighting::LinkGenerationError error) {
  content::RenderFrameHost* main_frame = web_contents()->GetPrimaryMainFrame();
  if (!main_frame || main_frame->GetGlobalId() != request.main_frame_id) {
    return {ScrollPositionGenerationOutcome::kMainFrameChanged, {}};
  }

  if (error == shared_highlighting::LinkGenerationError::kTimeout) {
    return {ScrollPositionGenerationOutcome::kRendererTimeout, {}};
  }

  if (error != shared_highlighting::LinkGenerationError::kNone) {
    return {ScrollPositionGenerationOutcome::kLinkGenerationError, {}};
  }

  if (selector.empty()) {
    return {ScrollPositionGenerationOutcome::kEmptySelector, {}};
  }

  std::optional<shared_highlighting::TextFragment> fragment =
      shared_highlighting::TextFragment::FromEscapedString(selector);
  if (!fragment) {
    return {ScrollPositionGenerationOutcome::kInvalidSelector, {}};
  }

  RecordScrollPositionSelectorLength(selector.length());
  ScrollPosition scroll_position;
  scroll_position.text_fragment = TextFragmentData(*fragment);
  return {ScrollPositionGenerationOutcome::kSuccess,
          std::move(scroll_position)};
}

void SendTabToSelfPageHandler::MaybeExtractFormFields(PendingRequest& request) {
  if (request.url == web_contents()->GetLastCommittedURL() &&
      base::FeatureList::IsEnabled(kSendTabToSelfPropagateFormFields)) {
    request.page_context.form_field_info =
        ExtractFormFieldsFromWebContents(web_contents());
  }
}

void SendTabToSelfPageHandler::MaybeExtractNavigationHistory(
    PendingRequest& request) {
  if (base::FeatureList::IsEnabled(kSendTabToSelfPropagateNavigationHistory)) {
    content::NavigationController& controller = web_contents()->GetController();
    std::vector<sessions::SerializedNavigationEntry> navigations;
    for (int i = 0; i < controller.GetEntryCount(); ++i) {
      navigations.push_back(
          sessions::ContentSerializedNavigationBuilder::FromNavigationEntry(
              i, controller.GetEntryAtIndex(i)));
    }
    request.navigation_history = NavigationHistory(
        std::move(navigations), controller.GetCurrentEntryIndex());
  }
}

void SendTabToSelfPageHandler::SendFinalizedRequest(
    PendingRequest request,
    std::optional<ScrollPositionGenerationOutcome> outcome) {
  if (outcome) {
    RecordScrollPositionGenerationOutcome(*outcome);
    RecordScrollPositionGenerationTime(base::TimeTicks::Now() -
                                       request.start_time);
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  SendTabToSelfModel* model =
      SendTabToSelfSyncServiceFactory::GetForProfile(profile)
          ->GetSendTabToSelfModel();
  if (!model->IsReady()) {
    std::move(request.commit_confirmation)
        .Run(SendTabToSelfResult::kFailureModelNotReady);
    return;
  }

  model->AddEntry(request.url, request.title, request.target_device_guid,
                  std::move(request.page_context),
                  std::move(request.navigation_history),
                  std::move(request.commit_confirmation));
}

base::TimeDelta SendTabToSelfPageHandler::GetSelectorGenerationTimeout() const {
  return selector_generation_timeout_for_testing_.is_zero()
             ? base::Milliseconds(200)
             : selector_generation_timeout_for_testing_;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SendTabToSelfPageHandler);

}  // namespace send_tab_to_self
