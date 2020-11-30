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
class ClipboardDataEndpoint;
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

  void OnGeneratedSelector(std::unique_ptr<ui::ClipboardDataEndpoint> endpoint,
                           const std::string& selector);
  // Convenience method for overriding the generated selector to bypass making
  // calls to the remote interface during tests.
  void OverrideGeneratedSelectorForTesting(const std::string& selector);

 private:
  explicit CopyLinkToTextMenuObserver(RenderViewContextMenuProxy* proxy);
  mojo::Remote<blink::mojom::TextFragmentSelectorProducer> remote_;
  RenderViewContextMenuProxy* proxy_;
  GURL url_;
  base::string16 selected_text_;
  base::Optional<std::string> generated_selector_for_testing_;
  base::WeakPtrFactory<CopyLinkToTextMenuObserver> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_RENDERER_CONTEXT_MENU_COPY_LINK_TO_TEXT_MENU_OBSERVER_H_
