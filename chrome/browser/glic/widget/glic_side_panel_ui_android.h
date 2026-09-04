// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_WIDGET_GLIC_SIDE_PANEL_UI_ANDROID_H_
#define CHROME_BROWSER_GLIC_WIDGET_GLIC_SIDE_PANEL_UI_ANDROID_H_

#include <set>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/glic/common/local_hotkey_manager.h"
#include "chrome/browser/glic/host/context/glic_screenshot_capturer.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_webui.mojom.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/glic_side_panel_coordinator.h"
#include "chrome/browser/glic/service/glic_ui_embedder.h"
#include "chrome/browser/pwc/privileged_web_contents.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "components/embedder_support/android/delegate/web_contents_delegate_android.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/media_stream_request.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-forward.h"
#include "ui/base/accelerators/accelerator.h"

class BrowserWindowInterface;
class GlobalBrowserCollection;
class Profile;

namespace tabs {
class TabInterface;
}

namespace blink::mojom {
class FileChooserParams;
}

namespace content {
class FileSelectListener;
class RenderFrameHost;
struct DropData;
}  // namespace content

namespace input {
struct NativeWebKeyboardEvent;
}

namespace glic {

class GlicInstanceMetrics;
class PanelVisibilityDependentHotkeyManager;
class PanelFocusDependentHotkeyManager;

class GlicSidePanelUi
    : public GlicUiEmbedder,
      public Host::EmbedderDelegate,
      public Host::Observer,
      public LocalHotkeyManager::Panel,
      public BrowserCollectionObserver,
      public web_contents_delegate_android::WebContentsDelegateAndroid,
      public pwc::PrivilegedWebContents::EmbedderDelegate {
 public:
  GlicSidePanelUi(Profile* profile,
                  base::WeakPtr<tabs::TabInterface> tab,
                  GlicUiEmbedder::Delegate& delegate,
                  GlicInstanceMetrics& instance_metrics);
  ~GlicSidePanelUi() override;

  // GlicUiEmbedder:
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
  void Attach() override;
  void Detach() override;
  void SwitchConversation(
      glic::mojom::ConversationInfoPtr info,
      mojom::WebClientHandler::SwitchConversationCallback callback) override;
  void CaptureScreenshot(
      glic::mojom::WebClientHandler::CaptureScreenshotCallback callback)
      override;

  // GlicUiEmbedder and Host::Delegate:
  bool IsShowing() const override;
  bool IsShowingOrBackgrounded() const override;
  void ClosePanel() override;
  void OnReload() override;
  void OnMicrophoneStatusChanged(mojom::MicrophoneStatus status) override {}

  // Host::Observer:
  void ActiveWebContentsChanged(content::WebContents* new_contents) override;

  // web_contents_delegate_android::WebContentsDelegateAndroid and
  // pwc::PrivilegedWebContents::EmbedderDelegate:
  void ContentsZoomChange(bool zoom_in) override;
  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      content::WebContents* source,
      const input::NativeWebKeyboardEvent& event) override;
  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;
  bool CanDragEnter(content::WebContents* source,
                    const content::DropData& data,
                    blink::DragOperationsMask operations_allowed) override;
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override;
  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const url::Origin& security_origin,
                                  blink::mojom::MediaStreamType type) override;
  void RunFileChooser(content::RenderFrameHost* render_frame_host,
                      scoped_refptr<content::FileSelectListener> listener,
                      const blink::mojom::FileChooserParams& params) override;
  void PrintCrossProcessSubframe(
      content::WebContents* web_contents,
      const gfx::Rect& rect,
      int document_cookie,
      content::RenderFrameHost* subframe_host) const override;

  // BrowserCollectionObserver
  void OnBrowserActivated(BrowserWindowInterface* browser) override;
  void OnBrowserDeactivated(BrowserWindowInterface* browser) override;

  void SidePanelStateChanged(GlicSidePanelCoordinator::State state);

  // LocalHotkeyManager::Panel:
  void FocusIfOpen() override;
  bool ActivateBrowser() override;
  void Zoom(mojom::ZoomAction action, ZoomSource source) override;
  BrowserWindowInterface* GetBrowserWindowInterface() override;

  PanelFocusDependentHotkeyManager*
  GetPanelFocusDependentHotkeyManagerForTesting() {
    return panel_focus_dependent_hotkey_manager_.get();
  }
  PanelVisibilityDependentHotkeyManager*
  GetPanelVisibilityDependentHotkeyManagerForTesting() {
    return panel_visibility_dependent_hotkey_manager_.get();
  }

 private:
  GlicSidePanelCoordinator* GetGlicSidePanelCoordinator() const;

  base::CallbackListSubscription panel_visibility_subscription_;
  base::ScopedObservation<GlobalBrowserCollection, BrowserCollectionObserver>
      browser_observation_{this};
  mojom::PanelState panel_state_;
  base::WeakPtr<tabs::TabInterface> tab_;
  const raw_ref<GlicUiEmbedder::Delegate> delegate_;
  const raw_ref<GlicInstanceMetrics> instance_metrics_;
  std::unique_ptr<PanelVisibilityDependentHotkeyManager>
      panel_visibility_dependent_hotkey_manager_;
  std::unique_ptr<PanelFocusDependentHotkeyManager>
      panel_focus_dependent_hotkey_manager_;
  raw_ptr<Profile> profile_;

  std::unique_ptr<GlicScreenshotCapturer> screenshot_capturer_;

  base::ScopedObservation<Host, Host::Observer> host_observation_{this};

  base::WeakPtrFactory<GlicSidePanelUi> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_WIDGET_GLIC_SIDE_PANEL_UI_ANDROID_H_
