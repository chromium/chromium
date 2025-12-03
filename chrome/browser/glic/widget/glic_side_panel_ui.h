// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_WIDGET_GLIC_SIDE_PANEL_UI_H_
#define CHROME_BROWSER_GLIC_WIDGET_GLIC_SIDE_PANEL_UI_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/glic/host/context/glic_screenshot_capturer.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/service/glic_ui_embedder.h"
#include "chrome/browser/glic/widget/glic_view.h"
#include "chrome/browser/glic/widget/local_hotkey_manager.h"
#include "chrome/browser/ui/views/side_panel/glic/glic_side_panel_coordinator.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/snapshot/snapshot.h"

namespace tabs {
class TabInterface;
}

#include "ui/views/widget/widget_observer.h"

namespace glic {

class GlicView;
class GlicInstanceMetrics;

// Implementation of GlicUiEmbedder for side panel UIs.
class GlicSidePanelUi
    : public GlicUiEmbedder,
      public Host::EmbedderDelegate,
      public LocalHotkeyManager::Panel,
      public web_modal::WebContentsModalDialogManagerDelegate {
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
  void Close() override;
  std::unique_ptr<GlicUiEmbedder> CreateInactiveEmbedder() const override;
  void Focus() override;
  mojom::PanelState GetPanelState() const override;
  gfx::Size GetPanelSize() override;
  std::string DescribeForTesting() override;

  // Host::EmbedderDelegate:
  void Resize(const gfx::Size& size,
              base::TimeDelta duration,
              base::OnceClosure callback) override;
  void SetDraggableAreas(
      const std::vector<gfx::Rect>& draggable_areas) override;
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

  void SidePanelStateChanged(GlicSidePanelCoordinator::State state);

  // LocalHotkeyManager::Panel:
  void FocusIfOpen() override;
  bool HasFocus() override;
  bool ActivateBrowser() override;
  void ShowTitleBarContextMenuAt(gfx::Point event_loc) override;
  base::WeakPtr<views::View> GetView() override;

  // web_modal::WebContentsModalDialogManagerDelegate:
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost(
      content::WebContents* web_contents) override;

 private:
  void OnBrowserWindowActivated(BrowserWindowInterface* bwi);
  void OnBrowserWindowDeactivated(BrowserWindowInterface* bwi);
  // Focuses on embedder's webcontens.
  void SetFocusDelayed();

  GlicSidePanelCoordinator* GetGlicSidePanelCoordinator() const;
  base::CallbackListSubscription panel_visibility_subscription_;
  std::unique_ptr<views::View> CreateView(Profile* profile);
  mojom::PanelState panel_state_;
  raw_ptr<Profile> profile_;
  base::WeakPtr<tabs::TabInterface> tab_;
  raw_ref<GlicUiEmbedder::Delegate> delegate_;
  raw_ref<GlicInstanceMetrics> instance_metrics_;
  base::WeakPtr<GlicView> glic_view_;
  std::unique_ptr<LocalHotkeyManager> application_hotkey_manager_;
  std::unique_ptr<LocalHotkeyManager> glic_panel_hotkey_manager_;
  base::CallbackListSubscription activation_subscription_;
  base::CallbackListSubscription deactivation_subscription_;

  std::unique_ptr<GlicScreenshotCapturer> screenshot_capturer_;

  base::WeakPtrFactory<GlicSidePanelUi> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_WIDGET_GLIC_SIDE_PANEL_UI_H_
