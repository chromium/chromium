// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/link_to_text_menu_observer.h"

#include <memory>

#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/renderer_context_menu/render_view_context_menu_proxy.h"
#include "components/shared_highlighting/core/common/disabled_sites.h"
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
constexpr char kTextFragmentUrlClassifier[] = "#:~:text=";

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

}  // namespace

// static
std::unique_ptr<LinkToTextMenuObserver> LinkToTextMenuObserver::Create(
    RenderViewContextMenuProxy* proxy,
    content::RenderFrameHost* render_frame_host) {
  // WebContents can be null in tests.
  content::WebContents* web_contents = proxy->GetWebContents();
  if (web_contents && extensions::ProcessManager::Get(
                          proxy->GetWebContents()->GetBrowserContext())
                          ->GetExtensionForWebContents(web_contents)) {
    // Do not show menu item for extensions, such as the PDF viewer.
    return nullptr;
  }

  DCHECK(render_frame_host);
  return base::WrapUnique(new LinkToTextMenuObserver(proxy, render_frame_host));
}

LinkToTextMenuObserver::LinkToTextMenuObserver(
    RenderViewContextMenuProxy* proxy,
    content::RenderFrameHost* render_frame_host) {
  proxy_ = proxy;
  render_frame_host_ = render_frame_host;
}
LinkToTextMenuObserver::~LinkToTextMenuObserver() = default;

void LinkToTextMenuObserver::InitMenu(
    const content::ContextMenuParams& params) {
  link_needs_generation_ = !params.selection_text.empty();
  raw_url_ = params.page_url;
  if (params.page_url.has_ref()) {
    GURL::Replacements replacements;
    replacements.ClearRef();
    url_ = params.page_url.ReplaceComponents(replacements);
  } else {
    url_ = params.page_url;
  }

  proxy_->AddMenuItem(
      IDC_CONTENT_CONTEXT_COPYLINKTOTEXT,
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_COPYLINKTOTEXT));
  if (params.opened_from_highlight) {
    proxy_->AddMenuItem(
        IDC_CONTENT_CONTEXT_REMOVELINKTOTEXT,
        l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_REMOVELINKTOTEXT));
  }

  if (link_needs_generation_) {
    RequestLinkGeneration();
  }
}

bool LinkToTextMenuObserver::IsCommandIdSupported(int command_id) {
  return command_id == IDC_CONTENT_CONTEXT_COPYLINKTOTEXT ||
         command_id == IDC_CONTENT_CONTEXT_REMOVELINKTOTEXT;
}

bool LinkToTextMenuObserver::IsCommandIdEnabled(int command_id) {
  // This should only be called for the command for copying link to text.
  DCHECK(IsCommandIdSupported(command_id));

  // If a link generation was needed, only enable the command if a link was
  // successfully generated.
  if (link_needs_generation_) {
    return generated_link_.has_value();
  }

  // For other cases (re-sharing and removing), the options are always enabled.
  return true;
}

void LinkToTextMenuObserver::ExecuteCommand(int command_id) {
  // This should only be called for the command for copying link to text.
  DCHECK(IsCommandIdSupported(command_id));

  if (command_id == IDC_CONTENT_CONTEXT_COPYLINKTOTEXT) {
    if (!link_needs_generation_) {
      ReshareLink();
    } else {
      CopyLinkToClipboard();
    }
  } else if (command_id == IDC_CONTENT_CONTEXT_REMOVELINKTOTEXT) {
    RemoveHighlights();
  }
}

void LinkToTextMenuObserver::OnRequestLinkGenerationCompleted(
    const std::string& selector,
    LinkGenerationError error,
    LinkGenerationReadyStatus ready_status) {
  is_generation_complete_ = true;
  LinkGenerationStatus status = selector.empty()
                                    ? LinkGenerationStatus::kFailure
                                    : LinkGenerationStatus::kSuccess;
  shared_highlighting::LogLinkRequestedBeforeStatus(status, ready_status);
  if (status == LinkGenerationStatus::kSuccess) {
    DCHECK_EQ(error, LinkGenerationError::kNone);
    shared_highlighting::LogRequestedSuccessMetrics();
  } else {
    DCHECK_NE(error, LinkGenerationError::kNone);
    shared_highlighting::LogRequestedFailureMetrics(error);

    // If there is no valid selector, leave the menu item disabled.
    return;
  }

  // Enable the menu option.
  generated_link_ = url_.spec() + kTextFragmentUrlClassifier + selector;
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
      proxy_->GetWebContents()->GetMainFrame();
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

  // Make a call to the renderer to generate a string that uniquely represents
  // the selected text and any context around the text to distinguish it from
  // the rest of the contents. |RequestSelector| will call a
  // |OnRequestLinkGenerationCompleted| callback with the generated string if it
  // succeeds or an empty string if it fails, along with error code and whether
  // the generation was completed at the time of the request.
  GetRemote()->RequestSelector(
      base::BindOnce(&LinkToTextMenuObserver::OnRequestLinkGenerationCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&LinkToTextMenuObserver::Timeout,
                     weak_ptr_factory_.GetWeakPtr()),
      timeout_length_ms);
}

void LinkToTextMenuObserver::CopyLinkToClipboard() {
  std::unique_ptr<ui::DataTransferEndpoint> data_transfer_endpoint =
      !render_frame_host_->GetBrowserContext()->IsOffTheRecord()
          ? std::make_unique<ui::DataTransferEndpoint>(
                render_frame_host_->GetMainFrame()->GetLastCommittedOrigin())
          : nullptr;

  ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste,
                                std::move(data_transfer_endpoint));
  scw.WriteText(base::UTF8ToUTF16(generated_link_.value()));

  LogDesktopLinkGenerationCopiedLinkType(
      shared_highlighting::LinkGenerationCopiedLinkType::
          kCopiedFromNewGeneration);

  // Log usage for Shared Highlighting promo.
  feature_engagement::TrackerFactory::GetForBrowserContext(
      proxy_->GetWebContents()->GetBrowserContext())
      ->NotifyEvent("iph_desktop_shared_highlighting_used");
}

void LinkToTextMenuObserver::Timeout() {
  DCHECK(remote_.is_bound());
  DCHECK(remote_.is_connected());
  if (is_generation_complete_)
    return;
  remote_->Cancel();
  remote_.reset();
  CompleteWithError(LinkGenerationError::kTimeout);
}

void LinkToTextMenuObserver::CompleteWithError(LinkGenerationError error) {
  is_generation_complete_ = true;
  shared_highlighting::LogRequestedFailureMetrics(error);
}

void LinkToTextMenuObserver::ReshareLink() {
  GetRemote()->GetExistingSelectors(
      base::BindOnce(&LinkToTextMenuObserver::OnGetExistingSelectorsComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void LinkToTextMenuObserver::OnGetExistingSelectorsComplete(
    const std::vector<std::string>& selectors) {
  std::unique_ptr<ui::DataTransferEndpoint> data_transfer_endpoint =
      !render_frame_host_->GetBrowserContext()->IsOffTheRecord()
          ? std::make_unique<ui::DataTransferEndpoint>(
                render_frame_host_->GetMainFrame()->GetLastCommittedOrigin())
          : nullptr;

  ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste,
                                std::move(data_transfer_endpoint));

  GURL url_to_share =
      shared_highlighting::RemoveFragmentSelectorDirectives(url_);
  url_to_share = shared_highlighting::AppendSelectors(url_to_share, selectors);

  scw.WriteText(base::UTF8ToUTF16(url_to_share.spec()));

  LogDesktopLinkGenerationCopiedLinkType(
      shared_highlighting::LinkGenerationCopiedLinkType::
          kCopiedFromExistingHighlight);
}

void LinkToTextMenuObserver::RemoveHighlights() {
  // Remove highlights from all frames in the primary page.
  proxy_->GetWebContents()->GetMainFrame()->ForEachRenderFrameHost(
      base::BindRepeating(RemoveHighlightsInFrame));
}

mojo::Remote<blink::mojom::TextFragmentReceiver>&
LinkToTextMenuObserver::GetRemote() {
  if (!remote_.is_bound()) {
    render_frame_host_->GetRemoteInterfaces()->GetInterface(
        remote_.BindNewPipeAndPassReceiver());
  }
  return remote_;
}
