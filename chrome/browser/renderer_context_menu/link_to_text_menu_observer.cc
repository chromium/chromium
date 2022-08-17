// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/link_to_text_menu_observer.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/grit/generated_resources.h"
#include "components/renderer_context_menu/render_view_context_menu_proxy.h"
#include "components/shared_highlighting/core/common/disabled_sites.h"
#include "components/shared_highlighting/core/common/shared_highlighting_features.h"
#include "components/shared_highlighting/core/common/shared_highlighting_metrics.h"
#include "components/shared_highlighting/core/common/text_fragments_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/process_manager.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
constexpr char kTextFragmentUrlClassifier[] = "#:~:text=";

// Indicates how long context menu should wait for link generation result.
constexpr base::TimeDelta kTimeoutMs = base::Milliseconds(500);
}  // namespace

// static
std::unique_ptr<LinkToTextMenuObserver> LinkToTextMenuObserver::Create(
    RenderViewContextMenuProxy* proxy,
    content::GlobalRenderFrameHostId render_frame_host_id) {
  // WebContents can be null in tests.
  content::WebContents* web_contents = proxy->GetWebContents();
  if (web_contents && extensions::ProcessManager::Get(
                          proxy->GetWebContents()->GetBrowserContext())
                          ->GetExtensionForWebContents(web_contents)) {
    // Do not show menu item for extensions, such as the PDF viewer.
    return nullptr;
  }

  DCHECK(content::RenderFrameHost::FromID(render_frame_host_id));
  return base::WrapUnique(
      new LinkToTextMenuObserver(proxy, render_frame_host_id));
}

LinkToTextMenuObserver::LinkToTextMenuObserver(
    RenderViewContextMenuProxy* proxy,
    content::GlobalRenderFrameHostId render_frame_host_id)
    : proxy_(proxy), render_frame_host_id_(render_frame_host_id) {}

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

  if (ShouldPreemptivelyGenerateLink()) {
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

  // If preemptively generating the link, only enable the command if the link
  // has already been successfully generated.
  if (ShouldPreemptivelyGenerateLink())
    return generated_link_.has_value();

  return true;
}

void LinkToTextMenuObserver::ExecuteCommand(int command_id) {
  // This should only be called for the command for copying link to text.
  DCHECK(IsCommandIdSupported(command_id));

  if (command_id == IDC_CONTENT_CONTEXT_COPYLINKTOTEXT) {
    if (!link_needs_generation_) {
      ReshareLink();
    } else {
      if (ShouldPreemptivelyGenerateLink()) {
        CopyLinkToClipboard();
      } else {
        RequestLinkGeneration();
      }
    }
  } else if (command_id == IDC_CONTENT_CONTEXT_REMOVELINKTOTEXT) {
    RemoveHighlight();
  }
}

void LinkToTextMenuObserver::OnRequestLinkGenerationCompleted(
    const std::string& selector) {
  is_generation_complete_ = true;
  if (ShouldPreemptivelyGenerateLink()) {
    if (selector.empty()) {
      // If there is no valid selector, leave the item disabled.
      return;
    }
    generated_link_ = url_.spec() + kTextFragmentUrlClassifier + selector;
    proxy_->UpdateMenuItem(
        IDC_CONTENT_CONTEXT_COPYLINKTOTEXT, true, false,
        l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_COPYLINKTOTEXT));
    return;
  }

  if (selector.empty())
    generated_link_ = url_.spec();
  else
    generated_link_ = url_.spec() + kTextFragmentUrlClassifier + selector;
  CopyLinkToClipboard();
}

void LinkToTextMenuObserver::OverrideGeneratedSelectorForTesting(
    const std::string& selector) {
  generated_selector_for_testing_ = url_.spec() + selector;
}

bool LinkToTextMenuObserver::ShouldPreemptivelyGenerateLink() {
  return base::FeatureList::IsEnabled(
             shared_highlighting::kPreemptiveLinkToTextGeneration) &&
         link_needs_generation_;
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
    shared_highlighting::LogGenerateErrorBlockList();
    OnRequestLinkGenerationCompleted(std::string());
    return;
  }

  // Check whether the selected text is in an iframe.
  if (main_frame != proxy_->GetWebContents()->GetFocusedFrame()) {
    shared_highlighting::LogGenerateErrorIFrame();
    OnRequestLinkGenerationCompleted(std::string());
    return;
  }

  if (generated_selector_for_testing_.has_value()) {
    OnRequestLinkGenerationCompleted(generated_selector_for_testing_.value());
    return;
  }

  base::TimeDelta timeout_length_ms =
      ShouldPreemptivelyGenerateLink()
          ? base::Milliseconds(
                shared_highlighting::GetPreemptiveLinkGenTimeoutLengthMs())
          : kTimeoutMs;

  // Make a call to the renderer to generate a string that uniquely represents
  // the selected text and any context around the text to distinguish it from
  // the rest of the contents. Get will call a callback with
  // the generated string if it succeeds or an empty string if it fails.
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
  auto* rfh = content::RenderFrameHost::FromID(render_frame_host_id_);
  CHECK(rfh);
  std::unique_ptr<ui::DataTransferEndpoint> data_transfer_endpoint =
      !rfh->GetBrowserContext()->IsOffTheRecord()
          ? std::make_unique<ui::DataTransferEndpoint>(
                rfh->GetLastCommittedOrigin())
          : nullptr;

  ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste,
                                std::move(data_transfer_endpoint));
  scw.WriteText(base::UTF8ToUTF16(generated_link_.value()));

  LogDesktopLinkGenerationCopiedLinkType(
      shared_highlighting::LinkGenerationCopiedLinkType::
          kCopiedFromNewGeneration);
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
  shared_highlighting::LogGenerateErrorTimeout();
  OnRequestLinkGenerationCompleted(std::string());
}

void LinkToTextMenuObserver::ReshareLink() {
  if (generated_selector_for_testing_.has_value()) {
    std::vector<std::string> test_selectors{
        generated_selector_for_testing_.value()};
    OnGetExistingSelectorsComplete(test_selectors);
    return;
  }

  GetRemote()->GetExistingSelectors(
      base::BindOnce(&LinkToTextMenuObserver::OnGetExistingSelectorsComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void LinkToTextMenuObserver::OnGetExistingSelectorsComplete(
    const std::vector<std::string>& selectors) {
  auto* rfh = content::RenderFrameHost::FromID(render_frame_host_id_);
  CHECK(rfh);
  std::unique_ptr<ui::DataTransferEndpoint> data_transfer_endpoint =
      !rfh->GetBrowserContext()->IsOffTheRecord()
          ? std::make_unique<ui::DataTransferEndpoint>(
                rfh->GetLastCommittedOrigin())
          : nullptr;

  ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste,
                                std::move(data_transfer_endpoint));

  GURL url_to_share = shared_highlighting::RemoveTextFragments(url_);
  url_to_share = shared_highlighting::AppendSelectors(url_to_share, selectors);

  scw.WriteText(base::UTF8ToUTF16(url_to_share.spec()));

  LogDesktopLinkGenerationCopiedLinkType(
      shared_highlighting::LinkGenerationCopiedLinkType::
          kCopiedFromExistingHighlight);
}

void LinkToTextMenuObserver::RemoveHighlight() {
  GetRemote()->RemoveFragments();
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
