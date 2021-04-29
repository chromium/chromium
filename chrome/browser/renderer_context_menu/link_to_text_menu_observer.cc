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
constexpr base::TimeDelta kTimeoutMs = base::TimeDelta::FromMilliseconds(500);
}  // namespace

// static
std::unique_ptr<LinkToTextMenuObserver> LinkToTextMenuObserver::Create(
    RenderViewContextMenuProxy* proxy) {
  // WebContents can be null in tests.
  content::WebContents* web_contents = proxy->GetWebContents();
  if (web_contents && extensions::ProcessManager::Get(
                          proxy->GetWebContents()->GetBrowserContext())
                          ->GetExtensionForWebContents(web_contents)) {
    // Do not show menu item for extensions, such as the PDF viewer.
    return nullptr;
  }

  return base::WrapUnique(new LinkToTextMenuObserver(proxy));
}

LinkToTextMenuObserver::LinkToTextMenuObserver(
    RenderViewContextMenuProxy* proxy)
    : proxy_(proxy) {}
LinkToTextMenuObserver::~LinkToTextMenuObserver() = default;

void LinkToTextMenuObserver::InitMenu(
    const content::ContextMenuParams& params) {
  highlight_exists_ = params.opened_from_highlight;
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

  if (ShouldPreemptivelyGenerateLink())
    RequestLinkGeneration();
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
    if (highlight_exists_) {
      CopyPageURLToClipboard();
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
         !highlight_exists_;
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
      kTimeoutMs);
}

void LinkToTextMenuObserver::CopyLinkToClipboard() {
  content::RenderFrameHost* main_frame =
      proxy_->GetWebContents()->GetMainFrame();

  std::unique_ptr<ui::DataTransferEndpoint> data_transfer_endpoint =
      main_frame ? std::make_unique<ui::DataTransferEndpoint>(
                       main_frame->GetLastCommittedOrigin())
                 : nullptr;

  ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste,
                                std::move(data_transfer_endpoint));
  scw.WriteText(base::UTF8ToUTF16(generated_link_.value()));
}

void LinkToTextMenuObserver::Timeout() {
  DCHECK(remote_.is_bound());
  DCHECK(remote_.is_connected());
  if (generated_link_.has_value())
    return;
  remote_->Cancel();
  remote_.reset();
  shared_highlighting::LogGenerateErrorTimeout();
  OnRequestLinkGenerationCompleted(std::string());
}

void LinkToTextMenuObserver::CopyPageURLToClipboard() {
  content::RenderFrameHost* main_frame =
      proxy_->GetWebContents()->GetMainFrame();

  std::unique_ptr<ui::DataTransferEndpoint> data_transfer_endpoint =
      main_frame ? std::make_unique<ui::DataTransferEndpoint>(
                       main_frame->GetLastCommittedOrigin())
                 : nullptr;

  ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste,
                                std::move(data_transfer_endpoint));
  scw.WriteText(
      base::UTF8ToUTF16(raw_url_.is_empty() ? std::string() : raw_url_.spec()));
}

void LinkToTextMenuObserver::RemoveHighlight() {
  GetRemote()->RemoveFragments();
}

mojo::Remote<blink::mojom::TextFragmentReceiver>&
LinkToTextMenuObserver::GetRemote() {
  if (!remote_.is_bound()) {
    content::RenderFrameHost* main_frame =
        proxy_->GetWebContents()->GetMainFrame();
    main_frame->GetRemoteInterfaces()->GetInterface(
        remote_.BindNewPipeAndPassReceiver());
  }
  return remote_;
}
