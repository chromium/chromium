// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/copy_link_to_text_menu_observer.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/grit/generated_resources.h"
#include "components/renderer_context_menu/render_view_context_menu_proxy.h"
#include "components/shared_highlighting/core/common/shared_highlighting_metrics.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/process_manager.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
constexpr char kTextFragmentUrlClassifier[] = "#:~:text=";
}

// static
std::unique_ptr<CopyLinkToTextMenuObserver> CopyLinkToTextMenuObserver::Create(
    RenderViewContextMenuProxy* proxy) {
  // WebContents can be null in tests.
  content::WebContents* web_contents = proxy->GetWebContents();
  if (web_contents && extensions::ProcessManager::Get(
                          proxy->GetWebContents()->GetBrowserContext())
                          ->GetExtensionForWebContents(web_contents)) {
    // Do not show menu item for extensions, such as the PDF viewer.
    return nullptr;
  }

  return base::WrapUnique(new CopyLinkToTextMenuObserver(proxy));
}

CopyLinkToTextMenuObserver::CopyLinkToTextMenuObserver(
    RenderViewContextMenuProxy* proxy)
    : proxy_(proxy) {}
CopyLinkToTextMenuObserver::~CopyLinkToTextMenuObserver() = default;

void CopyLinkToTextMenuObserver::InitMenu(
    const content::ContextMenuParams& params) {
  if (params.page_url.has_ref()) {
    GURL::Replacements replacements;
    replacements.ClearRef();
    url_ = params.page_url.ReplaceComponents(replacements);
  } else {
    url_ = params.page_url;
  }
  selected_text_ = params.selection_text;

  proxy_->AddMenuItem(
      IDC_CONTENT_CONTEXT_COPYLINKTOTEXT,
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_COPYLINKTOTEXT));
}

bool CopyLinkToTextMenuObserver::IsCommandIdSupported(int command_id) {
  return command_id == IDC_CONTENT_CONTEXT_COPYLINKTOTEXT;
}

bool CopyLinkToTextMenuObserver::IsCommandIdEnabled(int command_id) {
  // This should only be called for the command for copying link to text.
  DCHECK(IsCommandIdSupported(command_id));
  return true;
}

void CopyLinkToTextMenuObserver::ExecuteCommand(int command_id) {
  // This should only be called for the command for copying link to text.
  DCHECK(IsCommandIdSupported(command_id));

  if (generated_selector_for_testing_.has_value()) {
    OnGeneratedSelector(nullptr, generated_selector_for_testing_.value());
    return;
  }

  // Make a call to the renderer to generate a string that uniquely represents
  // the selected text and any context around the text to distinguish it from
  // the rest of the contents. GenerateSelector will call a callback with
  // the generated string if it succeeds or an empty string if it fails.
  content::RenderFrameHost* main_frame =
      proxy_->GetWebContents()->GetMainFrame();
  if (!main_frame)
    return;

  if (main_frame != proxy_->GetWebContents()->GetFocusedFrame()) {
    shared_highlighting::LogGenerateErrorIFrame();
    OnGeneratedSelector(std::make_unique<ui::ClipboardDataEndpoint>(
                            main_frame->GetLastCommittedOrigin()),
                        std::string());
    return;
  }

  main_frame->GetRemoteInterfaces()->GetInterface(
      remote_.BindNewPipeAndPassReceiver());
  remote_->GenerateSelector(
      base::BindOnce(&CopyLinkToTextMenuObserver::OnGeneratedSelector,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::make_unique<ui::ClipboardDataEndpoint>(
                         main_frame->GetLastCommittedOrigin())));
}

void CopyLinkToTextMenuObserver::OnGeneratedSelector(
    std::unique_ptr<ui::ClipboardDataEndpoint> endpoint,
    const std::string& selector) {
  ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste,
                                std::move(endpoint));
  std::string url = url_.spec();
  if (!selector.empty())
    url += kTextFragmentUrlClassifier + selector;
  scw.WriteText(base::UTF8ToUTF16("\"") + selected_text_ +
                base::UTF8ToUTF16("\"\n" + url));
}

void CopyLinkToTextMenuObserver::OverrideGeneratedSelectorForTesting(
    const std::string& selector) {
  generated_selector_for_testing_ = selector;
}
