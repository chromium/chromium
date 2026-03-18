// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/send_tab_to_self/send_tab_to_self_page_handler.h"

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/metrics_util.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/shared_highlighting/core/common/text_fragment.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace send_tab_to_self {

SendTabToSelfPageHandler::PendingRequest::PendingRequest()
    : start_time(base::TimeTicks::Now()) {}

SendTabToSelfPageHandler::PendingRequest::PendingRequest(PendingRequest&&) =
    default;

SendTabToSelfPageHandler::PendingRequest&
SendTabToSelfPageHandler::PendingRequest::operator=(PendingRequest&&) = default;

SendTabToSelfPageHandler::PendingRequest::~PendingRequest() = default;

SendTabToSelfPageHandler::SendTabToSelfPageHandler(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<SendTabToSelfPageHandler>(*web_contents) {}

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
    PageContext page_context,
    base::OnceClosure on_entry_added,
    base::OnceCallback<void(const GURL&)> on_send_failed) {
  PendingRequest request;
  request.target_device_guid = target_device_guid;
  request.url = url;
  request.title = title;
  request.page_context = std::move(page_context);
  request.on_entry_added = std::move(on_entry_added);
  request.on_send_failed = std::move(on_send_failed);

  if (!base::FeatureList::IsEnabled(kSendTabToSelfPropagateScrollPosition)) {
    SendFinalizedRequest(std::move(request), std::nullopt);
    return;
  }

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

  if (!text_fragment_receiver_.is_bound()) {
    main_frame->GetRemoteInterfaces()->GetInterface(
        text_fragment_receiver_.BindNewPipeAndPassReceiver());
  }

  auto request_token = base::Token::CreateRandom();
  pending_requests_[request_token] = std::move(request);

  text_fragment_receiver_->RequestSelectorForViewportCenter(
      base::BindOnce(&SendTabToSelfPageHandler::SelectorGeneratedForRequest,
                     weak_ptr_factory_.GetWeakPtr(), request_token,
                     /*is_browser_timeout=*/false));

  // A 200ms timeout is implemented to avoid delaying the sharing process. If
  // the selector generation takes longer, or if the page is navigated or
  // destroyed, the tab is sent without the scroll position information.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &SendTabToSelfPageHandler::SelectorGeneratedForRequest,
          weak_ptr_factory_.GetWeakPtr(), request_token,
          /*is_browser_timeout=*/true,
          /*selector=*/std::string(),
          shared_highlighting::LinkGenerationError::kTimeout,
          shared_highlighting::LinkGenerationReadyStatus::kRequestedAfterReady),
      GetSelectorGenerationTimeout());
}

void SendTabToSelfPageHandler::PrimaryPageChanged(content::Page& /*page*/) {
  for (auto& [token, request] : pending_requests_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&SendTabToSelfPageHandler::SelectorGeneratedForRequest,
                       weak_ptr_factory_.GetWeakPtr(), token,
                       /*is_browser_timeout=*/false,
                       /*selector=*/std::string(),
                       shared_highlighting::LinkGenerationError::kUnknown,
                       shared_highlighting::LinkGenerationReadyStatus::
                           kRequestedAfterReady));
  }
}

void SendTabToSelfPageHandler::WebContentsDestroyed() {
  pending_requests_.clear();
}

void SendTabToSelfPageHandler::SelectorGeneratedForRequest(
    base::Token request_token,
    bool is_browser_timeout,
    const std::string& selector,
    shared_highlighting::LinkGenerationError error,
    shared_highlighting::LinkGenerationReadyStatus /*ready_status*/) {
  auto it = pending_requests_.find(request_token);
  if (it == pending_requests_.end()) {
    // This happens if a request completes after the timeout has already
    // triggered, or if the timeout task runs after a request has already
    // completed.
    return;
  }

  PendingRequest request = std::move(it->second);
  pending_requests_.erase(it);

  ScrollPositionGenerationOutcome outcome =
      ScrollPositionGenerationOutcome::kSuccess;

  content::RenderFrameHost* main_frame = web_contents()->GetPrimaryMainFrame();
  if (!main_frame || main_frame->GetGlobalId() != request.main_frame_id) {
    outcome = ScrollPositionGenerationOutcome::kMainFrameChanged;
  } else if (is_browser_timeout) {
    outcome = ScrollPositionGenerationOutcome::kBrowserTimeout;
  } else if (error == shared_highlighting::LinkGenerationError::kTimeout) {
    outcome = ScrollPositionGenerationOutcome::kRendererTimeout;
  } else if (error != shared_highlighting::LinkGenerationError::kNone) {
    outcome = ScrollPositionGenerationOutcome::kLinkGenerationError;
  } else if (selector.empty()) {
    outcome = ScrollPositionGenerationOutcome::kEmptySelector;
  } else {
    std::optional<shared_highlighting::TextFragment> fragment =
        shared_highlighting::TextFragment::FromEscapedString(selector);
    if (fragment) {
      RecordScrollPositionSelectorLength(selector.length());
      request.page_context.scroll_position.text_fragment =
          TextFragmentData(*fragment);
    } else {
      outcome = ScrollPositionGenerationOutcome::kInvalidSelector;
    }
  }

  SendFinalizedRequest(std::move(request), outcome);
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
    if (request.on_send_failed) {
      std::move(request.on_send_failed).Run(request.url);
    }
    return;
  }

  model->AddEntry(request.url, request.title, request.target_device_guid,
                  std::move(request.page_context));

  if (request.on_entry_added) {
    std::move(request.on_entry_added).Run();
  }
}

void SendTabToSelfPageHandler::SetSelectorGenerationTimeoutForTesting(
    base::TimeDelta timeout) {
  selector_generation_timeout_for_testing_ = timeout;
}

base::TimeDelta SendTabToSelfPageHandler::GetSelectorGenerationTimeout() const {
  return selector_generation_timeout_for_testing_.is_zero()
             ? base::Milliseconds(200)
             : selector_generation_timeout_for_testing_;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SendTabToSelfPageHandler);

}  // namespace send_tab_to_self
