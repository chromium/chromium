// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_WIDGET_GLIC_SIDE_PANEL_UI_ANDROID_H_
#define CHROME_BROWSER_GLIC_WIDGET_GLIC_SIDE_PANEL_UI_ANDROID_H_

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/glic_side_panel_coordinator.h"
#include "chrome/browser/glic/service/glic_ui_embedder.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"

class BrowserWindowInterface;
class GlobalBrowserCollection;
class Profile;

namespace tabs {
class TabInterface;
}

namespace glic {

class GlicInstanceMetrics;

class GlicSidePanelUi : public GlicUiEmbedder,
                        public Host::EmbedderDelegate,
                        public BrowserCollectionObserver {
 public:
  GlicSidePanelUi(Profile* profile,
                  base::WeakPtr<tabs::TabInterface> tab,
                  GlicUiEmbedder::Delegate& delegate,
                  GlicInstanceMetrics& instance_metrics);
  ~GlicSidePanelUi() override;

  // GlicUiEmbedder:
  void OnClientReady() override;
  Host::EmbedderDelegate* GetHostEmbedderDelegate() override;
  void Show(const ShowOptions& options) override;
  void Close(const CloseOptions& options) override;
  void Focus() override;
  bool HasFocus() override;
  mojom::PanelState GetPanelState() const override;
  gfx::Size GetPanelSize() override;
  std::string DescribeForTesting() override;
  std::unique_ptr<GlicUiEmbedder> CreateInactiveEmbedder() const override;

  // Host::EmbedderDelegate:
  void Resize(const gfx::Size& size,
              base::TimeDelta duration,
              base::OnceClosure callback) override;
  void EnableDragResize(bool enabled) override;
  void Attach() override;
  void Detach() override;
  void SetMinimumWidgetSize(const gfx::Size& size) override;
  void SwitchConversation(
      glic::mojom::ConversationInfoPtr info,
      mojom::WebClientHandler::SwitchConversationCallback callback) override;
  void CaptureScreenshot(
      glic::mojom::WebClientHandler::CaptureScreenshotCallback callback)
      override;

  // GlicUiEmbedder and Host::Delegate:
  bool IsShowing() const override;
  void ClosePanel() override;
  void OnReload() override;
  void OnMicrophoneStatusChanged(mojom::MicrophoneStatus status) override {}

  // BrowserCollectionObserver
  void OnBrowserActivated(BrowserWindowInterface* browser) override;
  void OnBrowserDeactivated(BrowserWindowInterface* browser) override;

  void SidePanelStateChanged(GlicSidePanelCoordinator::State state);

 private:
  GlicSidePanelCoordinator* GetGlicSidePanelCoordinator() const;

  base::CallbackListSubscription panel_visibility_subscription_;
  base::ScopedObservation<GlobalBrowserCollection, BrowserCollectionObserver>
      browser_observation_{this};
  mojom::PanelState panel_state_;
  base::WeakPtr<tabs::TabInterface> tab_;
  const raw_ref<GlicUiEmbedder::Delegate> delegate_;
  const raw_ref<GlicInstanceMetrics> instance_metrics_;

  base::WeakPtrFactory<GlicSidePanelUi> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_WIDGET_GLIC_SIDE_PANEL_UI_ANDROID_H_
