// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_WIDGET_GLIC_TAB_UI_H_
#define CHROME_BROWSER_GLIC_WIDGET_GLIC_TAB_UI_H_

#include <memory>

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/glic/host/context/glic_screenshot_capturer.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/service/glic_ui_embedder.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"

class BrowserWindowInterface;

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace glic {

// Implementation of GlicUiEmbedder where Glic is displayed directly in a
// browser tab.
class GlicTabUi : public GlicUiEmbedder,
                  public Host::EmbedderDelegate,
                  public Host::Observer,
                  public BrowserCollectionObserver {
 public:
  GlicTabUi(base::WeakPtr<tabs::TabInterface> tab,
            GlicUiEmbedder::Delegate& delegate);
  ~GlicTabUi() override;

  // GlicUiEmbedder:
  Host::EmbedderDelegate* GetHostEmbedderDelegate() override;
  void Show(const ShowOptions& options) override;
  bool IsShowing() const override;
  bool IsShowingOrBackgrounded() const override;
  void Close(const CloseOptions& options) override;
  void Focus() override;
  bool HasFocus() override;
#if !BUILDFLAG(IS_ANDROID)
  base::WeakPtr<views::View> GetView() override;
#endif
  std::unique_ptr<GlicUiEmbedder> CreateInactiveEmbedder() const override;
  mojom::PanelState GetPanelState() const override;
  gfx::Size GetPanelSize() override;
  std::string DescribeForTesting() override;

  // Host::EmbedderDelegate:
  void Attach() override;
  void Detach() override;
  void ClosePanel() override;
  void OnReload() override;
  void CaptureScreenshot(
      glic::mojom::WebClientHandler::CaptureScreenshotCallback callback)
      override;
  void SwitchConversation(
      glic::mojom::ConversationInfoPtr info,
      mojom::WebClientHandler::SwitchConversationCallback callback) override;
  void OnMicrophoneStatusChanged(mojom::MicrophoneStatus status) override;

  // Host::Observer:
  void ActiveWebContentsChanged(content::WebContents* new_contents) override;

  // BrowserCollectionObserver:
  void OnBrowserActivated(BrowserWindowInterface* browser) override;
  void OnBrowserDeactivated(BrowserWindowInterface* browser) override;

 private:
  base::WeakPtr<tabs::TabInterface> tab_;
  raw_ref<GlicUiEmbedder::Delegate> delegate_;

  base::ScopedObservation<GlobalBrowserCollection, BrowserCollectionObserver>
      browser_observation_{this};
  base::ScopedObservation<Host, Host::Observer> host_observation_{this};

  std::unique_ptr<GlicScreenshotCapturer> screenshot_capturer_;

  base::WeakPtrFactory<GlicTabUi> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_WIDGET_GLIC_TAB_UI_H_
