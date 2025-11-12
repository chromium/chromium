// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SERVICE_GLIC_UI_EMBEDDER_H_
#define CHROME_BROWSER_GLIC_SERVICE_GLIC_UI_EMBEDDER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/glic/host/glic.mojom-forward.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/service/glic_ui_types.h"

namespace views {
class View;
}

namespace tabs {
class TabInterface;
}

namespace glic {

class GlicUiEmbedder {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void OnEmbedderWindowActivationChanged(bool has_focus) = 0;
    virtual void SwitchConversation(
        const ShowOptions& options,
        glic::mojom::ConversationInfoPtr info,
        mojom::WebClientHandler::SwitchConversationCallback callback) = 0;
    virtual void WillCloseFor(EmbedderKey key) = 0;
    virtual Host& host() = 0;
    virtual void Show(const ShowOptions& options) = 0;
    // Closes the side panel UI and opens the floating UI for this instance.
    virtual void Detach(tabs::TabInterface& tab) = 0;
    // Closes the floating UI for this instance and opens the side panel UI
    // in the tab that it was detached from. This should only be called from
    // GlicFloatingUi.
    virtual void Attach(tabs::TabInterface& tab) = 0;
    // Called after the value of GetPanelState() changes.
    virtual void NotifyPanelStateChanged() = 0;
  };

  virtual ~GlicUiEmbedder() = default;

  // Returns the Host::EmbedderDelegate if this embedder uses one.
  virtual Host::EmbedderDelegate* GetHostEmbedderDelegate() = 0;

  // Show the glic UI for this embedder. Do nothing if the embedder is
  // currently showing. Show will be called when switching from an inactive to
  // active embedder.
  virtual void Show(const ShowOptions& options) = 0;

  // Returns true if the embedder is currently showing.
  virtual bool IsShowing() const = 0;

  // Close the glic UI (keeps webclient alive for now)
  virtual void Close() = 0;

  // Focus embedder's webcontents.
  virtual void Focus() = 0;
  virtual bool HasFocus() = 0;

  // Returns the view, if there is one.
  virtual base::WeakPtr<views::View> GetView() = 0;

  // Creates the inactive version of this embedder.
  virtual std::unique_ptr<GlicUiEmbedder> CreateInactiveEmbedder() const = 0;

  // Returns the current panel state.
  virtual mojom::PanelState GetPanelState() const = 0;

  // Returns the size of the panel.
  virtual gfx::Size GetPanelSize() = 0;

  // Called when the client is ready to show.
  virtual void OnClientReady() {}

  virtual std::string DescribeForTesting() = 0;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SERVICE_GLIC_UI_EMBEDDER_H_
