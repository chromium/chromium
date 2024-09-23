// Copyright 2020 The Chromium Authors
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
class ToastController;

// A class that implements the menu item for copying selected text and a link
// to the selected text to the user's clipboard.
class LinkToTextMenuObserver : public RenderViewContextMenuObserver {
 public:
  static std::unique_ptr<LinkToTextMenuObserver> Create(
      RenderViewContextMenuProxy* proxy,
      content::GlobalRenderFrameHostId render_frame_host_id,
      ToastController* toast_controller);

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

  LinkToTextMenuObserver(RenderViewContextMenuProxy* proxy,
                         content::GlobalRenderFrameHostId render_frame_host_id,
                         ToastController* toast_controller);

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

  // Called when "Copy Link to Text" option is selected from a context menu for
  // a selected text.
  void ExecuteCopyLinkToText();

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

  // Copies given text to clipboard.
  void CopyTextToClipboard(const std::string& text);

  // Returns |remote_|, for the frame in which the context menu was opened.
  mojo::Remote<blink::mojom::TextFragmentReceiver>& GetRemote();

  mojo::Remote<blink::mojom::TextFragmentReceiver> remote_;
  raw_ptr<RenderViewContextMenuProxy> proxy_;
  raw_ptr<ToastController> const toast_controller_;

  GURL url_;
  GURL raw_url_;
  content::GlobalRenderFrameHostId render_frame_host_id_;

  std::unordered_map<content::GlobalRenderFrameHostId,
                     std::vector<std::string>,
                     content::GlobalRenderFrameHostIdHasher>
      frames_selectors_;

  std::vector<content::GlobalRenderFrameHostId> render_frame_host_ids_;
  std::vector<mojo::Remote<blink::mojom::TextFragmentReceiver>>
      text_fragment_remotes_;
  size_t get_frames_existing_selectors_counter_;

  // True when the context menu was opened with text selected.
  bool open_from_new_selection_ = false;

  std::optional<std::string> generated_link_;

  // True when generation is completed.
  bool is_generation_complete_ = false;

  base::WeakPtrFactory<LinkToTextMenuObserver> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_RENDERER_CONTEXT_MENU_LINK_TO_TEXT_MENU_OBSERVER_H_
