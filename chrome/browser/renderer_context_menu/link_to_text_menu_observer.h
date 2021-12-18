// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_CONTEXT_MENU_LINK_TO_TEXT_MENU_OBSERVER_H_
#define CHROME_BROWSER_RENDERER_CONTEXT_MENU_LINK_TO_TEXT_MENU_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "components/renderer_context_menu/render_view_context_menu_observer.h"
#include "components/shared_highlighting/core/common/shared_highlighting_metrics.h"
#include "content/public/browser/render_frame_host.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/link_to_text/link_to_text.mojom.h"
#include "url/gurl.h"

class RenderViewContextMenuProxy;

// A class that implements the menu item for copying selected text and a link
// to the selected text to the user's clipboard.
class LinkToTextMenuObserver : public RenderViewContextMenuObserver {
 public:
  static std::unique_ptr<LinkToTextMenuObserver> Create(
      RenderViewContextMenuProxy* proxy,
      content::RenderFrameHost* render_frame_host);

  LinkToTextMenuObserver(const LinkToTextMenuObserver&) = delete;
  LinkToTextMenuObserver& operator=(const LinkToTextMenuObserver&) = delete;
  ~LinkToTextMenuObserver() override;

  // RenderViewContextMenuObserver.
  void InitMenu(const content::ContextMenuParams& params) override;
  bool IsCommandIdSupported(int command_id) override;
  bool IsCommandIdEnabled(int command_id) override;
  void ExecuteCommand(int command_id) override;

  // Used in tests for waiting and receiving generation result.
  static void RegisterGenerationCompleteCallbackForTesting(
      base::OnceCallback<void(const std::string& selector)> cb);

 private:
  friend class MockLinkToTextMenuObserver;

  explicit LinkToTextMenuObserver(RenderViewContextMenuProxy* proxy,
                                  content::RenderFrameHost* render_frame_host);
  // Returns true if the link should be generated from the constructor, vs
  // determined when executed.
  bool ShouldPreemptivelyGenerateLink();

  // Requests link generation if needed.
  void RequestLinkGeneration();

  // Make an async request to the renderer to generate the link to text.
  // (virtual so it can be mocked in tests).
  virtual void StartLinkGenerationRequestWithTimeout();

  // Callback after the request to generate the selector has completed. This is
  // called with an empty selector if the generation failed or was called on
  // an invalid selection.
  void OnRequestLinkGenerationCompleted(
      const std::string& selector,
      shared_highlighting::LinkGenerationError error,
      shared_highlighting::LinkGenerationReadyStatus ready_status);

  // Copies the generated link to the user's clipboard.
  void CopyLinkToClipboard();

  // Make a request to the renderer to retrieve the selector for an
  // existing highlight.
  // (virtual so it can be mocked in tests).
  virtual void ReshareLink();

  // Callback after the request to retrieve an existing selector
  // is complete.
  void OnGetExistingSelectorsComplete(
      const std::vector<std::string>& selectors);

  // Removes the highlight from the page and updates the URL.
  void RemoveHighlights();

  // Cancels link generation if we are still waiting for it.
  void Timeout();

  // Completes necessary tasks when link to text was not generated or generation
  // was unsuccessful.
  void CompleteWithError(shared_highlighting::LinkGenerationError error);

  // Returns |remote_|, for the frame in which the context menu was opened.
  mojo::Remote<blink::mojom::TextFragmentReceiver>& GetRemote();

  mojo::Remote<blink::mojom::TextFragmentReceiver> remote_;
  raw_ptr<RenderViewContextMenuProxy> proxy_;
  GURL url_;
  GURL raw_url_;
  raw_ptr<content::RenderFrameHost> render_frame_host_;

  // True when the context menu was opened with text selected.
  bool link_needs_generation_ = false;

  absl::optional<std::string> generated_link_;

  // True when generation is completed.
  bool is_generation_complete_ = false;

  base::WeakPtrFactory<LinkToTextMenuObserver> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_RENDERER_CONTEXT_MENU_LINK_TO_TEXT_MENU_OBSERVER_H_
