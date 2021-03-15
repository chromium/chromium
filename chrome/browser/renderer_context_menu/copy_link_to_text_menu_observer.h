// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_CONTEXT_MENU_COPY_LINK_TO_TEXT_MENU_OBSERVER_H_
#define CHROME_BROWSER_RENDERER_CONTEXT_MENU_COPY_LINK_TO_TEXT_MENU_OBSERVER_H_

#include "components/renderer_context_menu/render_view_context_menu_observer.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/link_to_text/link_to_text.mojom.h"
#include "url/gurl.h"

class RenderViewContextMenuProxy;
namespace ui {
class DataTransferEndpoint;
}

// A class that implements the menu item for copying selected text and a link
// to the selected text to the user's clipboard.
class CopyLinkToTextMenuObserver : public RenderViewContextMenuObserver {
 public:
  static std::unique_ptr<CopyLinkToTextMenuObserver> Create(
      RenderViewContextMenuProxy* proxy);

  CopyLinkToTextMenuObserver(const CopyLinkToTextMenuObserver&) = delete;
  CopyLinkToTextMenuObserver& operator=(const CopyLinkToTextMenuObserver&) =
      delete;
  ~CopyLinkToTextMenuObserver() override;

  // RenderViewContextMenuObserver.
  void InitMenu(const content::ContextMenuParams& params) override;
  bool IsCommandIdSupported(int command_id) override;
  bool IsCommandIdEnabled(int command_id) override;
  void ExecuteCommand(int command_id) override;

  // Convenience method for overriding the generated selector to bypass making
  // calls to the remote interface during tests.
  void OverrideGeneratedSelectorForTesting(const std::string& selector);

 private:
  explicit CopyLinkToTextMenuObserver(RenderViewContextMenuProxy* proxy);
  // Returns true if the link should be generated from the constructor, vs
  // determined when executed.
  bool ShouldPreemptivelyGenerateLink();

  // Make an async request to the renderer to generate the link to text.
  void RequestLinkGeneration();

  // Callback after the request to generate the selector has completed. This is
  // called with an empty selector if the generation failed or was called on
  // an invalid selection.
  void OnRequestLinkGenerationCompleted(const std::string& selector);

  // Copies the generated link to the user's clipboard.
  void CopyLinkToClipboard();

  // Cancels link generation if we are still waiting for it.
  void Timeout();

  mojo::Remote<blink::mojom::TextFragmentSelectorProducer> remote_;
  RenderViewContextMenuProxy* proxy_;
  GURL url_;
  base::Optional<std::string> generated_link_;
  base::Optional<std::string> generated_selector_for_testing_;
  std::unique_ptr<ui::DataTransferEndpoint> data_transfer_endpoint_;

  base::WeakPtrFactory<CopyLinkToTextMenuObserver> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_RENDERER_CONTEXT_MENU_COPY_LINK_TO_TEXT_MENU_OBSERVER_H_
