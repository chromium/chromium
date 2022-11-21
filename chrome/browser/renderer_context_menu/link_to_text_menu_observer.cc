// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/link_to_text_menu_observer.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/renderer_context_menu/render_view_context_menu_proxy.h"
#include "components/shared_highlighting/core/common/disabled_sites.h"
#include "components/shared_highlighting/core/common/fragment_directives_constants.h"
#include "components/shared_highlighting/core/common/fragment_directives_utils.h"
#include "components/shared_highlighting/core/common/shared_highlighting_features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/process_manager.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/l10n/l10n_util.h"

using shared_highlighting::LinkGenerationError;
using shared_highlighting::LinkGenerationReadyStatus;
using shared_highlighting::LinkGenerationStatus;

namespace {

// Removes the highlight from the frame.
void RemoveHighlightsInFrame(content::RenderFrameHost* render_frame_host) {
  mojo::Remote<blink::mojom::TextFragmentReceiver> remote;

  // A TextFragmentReceiver is created lazily for each frame
  render_frame_host->GetRemoteInterfaces()->GetInterface(
      remote.BindNewPipeAndPassReceiver());
  remote->RemoveFragments();
}

base::OnceCallback<void(const std::string& selector)>*
GetGenerationCompleteCallbackForTesting() {
  static base::NoDestructor<
      base::OnceCallback<void(const std::string& selector)>>
      callback;
  return callback.get();
}

std::vector<std::string> GetAggregatedSelectors(
    std::unordered_map<content::GlobalRenderFrameHostId,
                       std::vector<std::string>,
                       content::GlobalRenderFrameHostIdHasher> frames_selectors,
    std::vector<content::GlobalRenderFrameHostId> render_frame_host_ids) {
  std::vector<std::string> aggregated_selectors;
  for (content::GlobalRenderFrameHostId render_frame_host_id :
       render_frame_host_ids) {
    std::vector<std::string>& frame_selectors =
        frames_selectors.at(render_frame_host_id);
    aggregated_selectors.insert(aggregated_selectors.end(),
                                frame_selectors.begin(), frame_selectors.end());
  }
  return aggregated_selectors;
}

}  // namespace

// static
std::unique_ptr<LinkToTextMenuObserver> LinkToTextMenuObserver::Create(
    RenderViewContextMenuProxy* proxy,
    content::GlobalRenderFrameHostId render_frame_host_id,
    CompletionCallback callback) {
  // WebContents can be null in tests.
  content::WebContents* web_contents = proxy->GetWebContents();
  if (web_contents && extensions::ProcessManager::Get(
                          proxy->GetWebContents()->GetBrowserContext())
                          ->GetExtensionForWebContents(web_contents)) {
    // Do not show menu item for extensions, such as the PDF viewer.
    return nullptr;
  }

  DCHECK(content::RenderFrameHost::FromID(render_frame_host_id));
  return base::WrapUnique(new LinkToTextMenuObserver(
      proxy, render_frame_host_id, std::move(callback)));
}

LinkToTextMenuObserver::LinkToTextMenuObserver(
    RenderViewContextMenuProxy* proxy,
    content::GlobalRenderFrameHostId render_frame_host_id,
    CompletionCallback callback)
    : proxy_(proxy),
      render_frame_host_id_(render_frame_host_id),
      completion_callback_(std::move(callback)) {}

LinkToTextMenuObserver::~LinkToTextMenuObserver() = default;

void LinkToTextMenuObserver::InitMenu(
    const content::ContextMenuParams& params) {
  open_from_new_selection_ = !params.selection_text.empty();
  raw_url_ = params.page_url;
  if (params.page_url.has_ref()) {
    url_ = shared_highlighting::RemoveFragmentSelectorDirectives(raw_url_);
  } else {
    url_ = params.page_url;
  }

  // It is possible that there is a new text selection on top of a highlight, in
  // which case, both open_from_new_selection_ and opened_from_highlight are
  // true. Consequently, a context menu for new text selection is created.
  if (open_from_new_selection_) {
    proxy_->AddMenuItem(
        IDC_CONTENT_CONTEXT_COPYLINKTOTEXT,
        l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_COPYLINKTOTEXT));
    RequestLinkGeneration();
  } else if (params.opened_from_highlight) {
    proxy_->AddMenuItem(
        IDC_CONTENT_CONTEXT_RESHARELINKTOTEXT,
        l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_RESHARELINKTOTEXT));
    proxy_->AddMenuItem(
        IDC_CONTENT_CONTEXT_REMOVELINKTOTEXT,
        l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_REMOVELINKTOTEXT));
  }
}

bool LinkToTextMenuObserver::IsCommandIdSupported(int command_id) {
  return command_id == IDC_CONTENT_CONTEXT_COPYLINKTOTEXT ||
         command_id == IDC_CONTENT_CONTEXT_REMOVELINKTOTEXT ||
         command_id == IDC_CONTENT_CONTEXT_RESHARELINKTOTEXT;
}

bool LinkToTextMenuObserver::IsCommandIdEnabled(int command_id) {
  // This should only be called for the command for copying link to text.
  DCHECK(IsCommandIdSupported(command_id));

  // If a link generation was needed, only enable the command if a link was
  // successfully generated.
  if (open_from_new_selection_) {
    return generated_link_.has_value();
  }

  // For other cases (re-sharing and removing), the options are always enabled.
  return true;
}

void LinkToTextMenuObserver::ExecuteCommand(int command_id) {
  // This should only be called for the command for copying link to text.
  DCHECK(IsCommandIdSupported(command_id));

  execute_command_pending_ = false;

  if (command_id == IDC_CONTENT_CONTEXT_COPYLINKTOTEXT) {
    ExecuteCopyLinkToText();
  } else if (command_id == IDC_CONTENT_CONTEXT_RESHARELINKTOTEXT) {
    ReshareLink();
  } else if (command_id == IDC_CONTENT_CONTEXT_REMOVELINKTOTEXT) {
    RemoveHighlights();
  }
}

void LinkToTextMenuObserver::OnMenuClosed() {
  is_menu_closed_ = true;
  NotifyLinkToTextMenuCompleted();
}

void LinkToTextMenuObserver::OnRequestLinkGenerationCompleted(
    const std::string& selector,
    LinkGenerationError error,
    LinkGenerationReadyStatus ready_status) {
  is_generation_complete_ = true;
  LinkGenerationStatus status = selector.empty()
                                    ? LinkGenerationStatus::kFailure
                                    : LinkGenerationStatus::kSuccess;

  // If the RenderFrameHost is no longer in the frame tree since the request was
  // issued, mark the request as a failure to ensure the RenderFrameHost isn't
  // used later for UKM.
  auto* rfh = content::RenderFrameHost::FromID(render_frame_host_id_);
  if (!rfh && status == LinkGenerationStatus::kSuccess) {
    status = LinkGenerationStatus::kFailure;
    error = LinkGenerationError::kUnknown;
  }

  shared_highlighting::LogLinkRequestedBeforeStatus(status, ready_status);

  if (status == LinkGenerationStatus::kSuccess) {
    DCHECK_EQ(error, LinkGenerationError::kNone);
    DCHECK(rfh);
    shared_highlighting::LogRequestedSuccessMetrics(rfh->GetPageUkmSourceId());
  } else {
    DCHECK_NE(error, LinkGenerationError::kNone);
    CompleteWithError(error);

    // If there is no valid selector, leave the menu item disabled.
    return;
  }

  // Enable the menu option.

  generated_link_ =
      shared_highlighting::AppendSelectors(url_, {selector}).spec();
  proxy_->UpdateMenuItem(
      IDC_CONTENT_CONTEXT_COPYLINKTOTEXT, true, false,
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_COPYLINKTOTEXT));

  // Useful only for testing to be notified when generation is complete.
  auto* cb = GetGenerationCompleteCallbackForTesting();
  if (!cb->is_null())
    std::move(*cb).Run(selector);
}

// static
void LinkToTextMenuObserver::RegisterGenerationCompleteCallbackForTesting(
    base::OnceCallback<void(const std::string& selector)> cb) {
  *GetGenerationCompleteCallbackForTesting() = std::move(cb);
}

void LinkToTextMenuObserver::RequestLinkGeneration() {
  content::RenderFrameHost* main_frame =
      proxy_->GetWebContents()->GetPrimaryMainFrame();
  if (!main_frame)
    return;

  // Check whether current url is blocklisted for link to text generation. This
  // check should happen before iframe check so that if both conditions are
  // present then blocklist error is logged.
  if (!shared_highlighting::ShouldOfferLinkToText(url_)) {
    CompleteWithError(LinkGenerationError::kBlockList);
    return;
  }

  // Check whether the selected text is in an iframe.
  if (main_frame != proxy_->GetWebContents()->GetFocusedFrame()) {
    CompleteWithError(LinkGenerationError::kIFrame);
    return;
  }

  StartLinkGenerationRequestWithTimeout();
}

void LinkToTextMenuObserver::StartLinkGenerationRequestWithTimeout() {
  base::TimeDelta timeout_length_ms = base::Milliseconds(
      shared_highlighting::GetPreemptiveLinkGenTimeoutLengthMs());

  // Make a call to the renderer to request generated selector that uniquely
  // represents the selected text and any context around the text to distinguish
  // it from the rest of the contents. |RequestSelector| will call a
  // |OnRequestLinkGenerationCompleted| callback with the generated string if it
  // succeeds or an empty string if it fails, along with error code and whether
  // the generation was completed at the time of the request.
  GetRemote()->RequestSelector(
      base::BindOnce(&LinkToTextMenuObserver::OnRequestLinkGenerationCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&LinkToTextMenuObserver::Timeout,
                     weak_ptr_factory_.GetWeakPtr()),
      timeout_length_ms);
}

void LinkToTextMenuObserver::ExecuteCopyLinkToText() {
  DCHECK(generated_link_.has_value());

  CopyTextToClipboard(generated_link_.value());

  LogDesktopLinkGenerationCopiedLinkType(
      shared_highlighting::LinkGenerationCopiedLinkType::
          kCopiedFromNewGeneration);

  // Log usage for Shared Highlighting promo.
  feature_engagement::TrackerFactory::GetForBrowserContext(
      proxy_->GetWebContents()->GetBrowserContext())
      ->NotifyEvent("iph_desktop_shared_highlighting_used");

  execute_command_pending_ = false;
  NotifyLinkToTextMenuCompleted();
}

void LinkToTextMenuObserver::Timeout() {
  auto* rfh = content::RenderFrameHost::FromID(render_frame_host_id_);
  // The renderer may remove the frame. Or it may have crashed leaving the
  // remote disconnected with the Timeout task still queued.
  if (rfh && rfh->IsRenderFrameLive()) {
    CHECK(remote_.is_connected());
    if (is_generation_complete_)
      return;
    remote_->Cancel();
    remote_.reset();
  }
  CompleteWithError(LinkGenerationError::kTimeout);
}

void LinkToTextMenuObserver::CompleteWithError(LinkGenerationError error) {
  is_generation_complete_ = true;
  auto* rfh = content::RenderFrameHost::FromID(render_frame_host_id_);
  if (rfh) {
    shared_highlighting::LogRequestedFailureMetrics(rfh->GetPageUkmSourceId(),
                                                    error);
  }

  execute_command_pending_ = false;
  NotifyLinkToTextMenuCompleted();
}

void LinkToTextMenuObserver::ReshareLink() {
  // Get the list of RenderFrameHosts from the current page.
  proxy_->GetWebContents()->GetPrimaryMainFrame()->ForEachRenderFrameHost(
      [this](content::RenderFrameHost* rfh) {
        render_frame_host_ids_.push_back(rfh->GetGlobalId());
        mojo::Remote<blink::mojom::TextFragmentReceiver> remote;
        text_fragment_remotes_.push_back(std::move(remote));
      });

  get_frames_existing_selectors_counter_ = render_frame_host_ids_.size();

  for (size_t i = 0; i < render_frame_host_ids_.size(); i++) {
    content::GlobalRenderFrameHostId render_frame_host_id(
        render_frame_host_ids_.at(i));
    content::RenderFrameHost* render_frame_host(
        content::RenderFrameHost::FromID(render_frame_host_id));
    render_frame_host->GetRemoteInterfaces()->GetInterface(
        text_fragment_remotes_.at(i).BindNewPipeAndPassReceiver());

    text_fragment_remotes_.at(i)->GetExistingSelectors(base::BindOnce(
        [](const base::WeakPtr<LinkToTextMenuObserver>&
               link_to_text_menu_observer_ptr,
           const content::GlobalRenderFrameHostId global_render_frame_host_id,
           const std::vector<std::string>& selectors) {
          if (link_to_text_menu_observer_ptr == nullptr)
            return;

          link_to_text_menu_observer_ptr->frames_selectors_.insert(
              {global_render_frame_host_id, selectors});

          link_to_text_menu_observer_ptr
              ->get_frames_existing_selectors_counter_--;

          if (link_to_text_menu_observer_ptr
                  ->get_frames_existing_selectors_counter_ == 0) {
            std::vector<std::string> aggregated_selectors =
                GetAggregatedSelectors(
                    link_to_text_menu_observer_ptr->frames_selectors_,
                    link_to_text_menu_observer_ptr->render_frame_host_ids_);
            link_to_text_menu_observer_ptr->OnGetExistingSelectorsComplete(
                aggregated_selectors);
          }
        },
        weak_ptr_factory_.GetWeakPtr(), render_frame_host_id));
  }
}

void LinkToTextMenuObserver::OnGetExistingSelectorsComplete(
    const std::vector<std::string>& aggregated_selectors) {
  GURL url_to_share =
      shared_highlighting::RemoveFragmentSelectorDirectives(url_);
  url_to_share =
      shared_highlighting::AppendSelectors(url_to_share, aggregated_selectors);

  CopyTextToClipboard(url_to_share.spec());

  LogDesktopLinkGenerationCopiedLinkType(
      shared_highlighting::LinkGenerationCopiedLinkType::
          kCopiedFromExistingHighlight);

  execute_command_pending_ = false;
  NotifyLinkToTextMenuCompleted();
}

void LinkToTextMenuObserver::RemoveHighlights() {
  // Remove highlights from all frames in the primary page.
  proxy_->GetWebContents()->GetPrimaryMainFrame()->ForEachRenderFrameHost(
      &RemoveHighlightsInFrame);

  execute_command_pending_ = false;
  NotifyLinkToTextMenuCompleted();
}

mojo::Remote<blink::mojom::TextFragmentReceiver>&
LinkToTextMenuObserver::GetRemote() {
  if (!remote_.is_bound()) {
    auto* rfh = content::RenderFrameHost::FromID(render_frame_host_id_);
    CHECK(rfh);
    rfh->GetRemoteInterfaces()->GetInterface(
        remote_.BindNewPipeAndPassReceiver());
  }
  return remote_;
}

void LinkToTextMenuObserver::CopyTextToClipboard(const std::string& text) {
  auto* rfh = content::RenderFrameHost::FromID(render_frame_host_id_);
  CHECK(rfh);

  std::unique_ptr<ui::DataTransferEndpoint> data_transfer_endpoint =
      !rfh->GetBrowserContext()->IsOffTheRecord()
          ? std::make_unique<ui::DataTransferEndpoint>(
                rfh->GetMainFrame()->GetLastCommittedURL())
          : nullptr;

  ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste,
                                std::move(data_transfer_endpoint));
  scw.WriteText(base::UTF8ToUTF16(text));
}

void LinkToTextMenuObserver::NotifyLinkToTextMenuCompleted() {
  // LinkToTextMenuObserver is not needed anymore if menu is closed and no
  // commands are being executed.
  if (is_menu_closed_ && !execute_command_pending_) {
    // Calling completion callback will destroy this object.
    std::move(completion_callback_).Run();
  }
}
