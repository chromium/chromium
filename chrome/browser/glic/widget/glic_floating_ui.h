// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_WIDGET_GLIC_FLOATING_UI_H_
#define CHROME_BROWSER_GLIC_WIDGET_GLIC_FLOATING_UI_H_

#include "base/callback_list.h"
#include "base/time/time.h"
#include "chrome/browser/glic/host/context/glic_screenshot_capturer.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/service/glic_ui_embedder.h"
#include "chrome/browser/glic/widget/glic_window_event_observer.h"
#include "chrome/browser/glic/widget/local_hotkey_manager.h"
#include "components/tabs/public/tab_interface.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_observer.h"

class BrowserWindowInterface;
class SkRegion;

namespace glic {

class GlicWindowAnimator;
class GlicWidget;
class GlicView;
class LocalHotkeyManager;
class GlicInstanceMetrics;

// A stub implementation of GlicUiEmbedder for floating UIs.
class GlicFloatingUi : public GlicUiEmbedder,
                       public Host::EmbedderDelegate,
                       public GlicWindowEventObserver::Delegate,
                       public LocalHotkeyManager::Panel,
                       public views::WidgetObserver,
                       public web_modal::WebContentsModalDialogManagerDelegate,
                       public web_modal::WebContentsModalDialogHost {
 public:
  GlicFloatingUi(Profile* profile,
                 BrowserWindowInterface* browser,
                 GlicUiEmbedder::Delegate& delegate,
                 GlicInstanceMetrics& instance_metrics);
  GlicFloatingUi(Profile* profile,
                 gfx::Rect initial_bounds,
                 tabs::TabHandle source_tab,
                 GlicUiEmbedder::Delegate& delegate,
                 GlicInstanceMetrics& instance_metrics);
  ~GlicFloatingUi() override;

  static gfx::Size GetDefaultSize();

  // GlicUiEmbedder:
  void OnClientReady() override;
  Host::EmbedderDelegate* GetHostEmbedderDelegate() override;
  void Show(const ShowOptions& options) override;
  bool IsShowing() const override;
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
  void SetDraggableRegion(const SkRegion& draggable_region) override;
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
  void ClosePanel() override;

  // GlicWindowEventObserver::Delegate:
  GlicWindowAnimator* window_animator() override;
  void OnDragComplete() override;

  // views::WidgetObserver implementation, monitoring the glic window widget.
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  void OnWidgetDestroyed(views::Widget* widget) override;
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;
  void OnWidgetUserResizeStarted() override;
  void OnWidgetUserResizeEnded() override;

  // LocalHotkeyManager::Panel:
  void FocusIfOpen() override;
  bool HasFocus() override;
  bool ActivateBrowser() override;
  void ShowTitleBarContextMenuAt(gfx::Point event_loc) override;
  base::WeakPtr<views::View> GetView() override;

  // web_modal::WebContentsModalDialogManagerDelegate:
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost(
      content::WebContents* web_contents) override;

  // web_modal::WebContentsModalDialogHost:
  gfx::Size GetMaximumDialogSize() override;
  gfx::NativeView GetHostView() const override;
  gfx::Point GetDialogPosition(const gfx::Size& dialog_size) override;
  bool ShouldConstrainDialogBoundsByHost() override;
  void AddObserver(web_modal::ModalDialogHostObserver* observer) override;
  void RemoveObserver(web_modal::ModalDialogHostObserver* observer) override;

 private:
  void ClearWebContentsDelegate();
  GlicWidget* GetGlicWidget() const;
  GlicView* GetGlicView() const;
  void CreateAndSetupWidget(gfx::Rect initial_bounds);
  void MaybeSetWidgetCanResize();
  void SetGlicWindowToFloatingMode(bool floating);
  void OnSourceTabDestroyed(tabs::TabInterface* tab,
                            const InstanceId& instance_id);
  void FloatingPanelCanAttachChanged(bool can_attach);

  // Whether the widget should be user resizable, kept here in case it's
  // specified before the widget is created.
  bool user_resizable_ = true;
  // Whether the user is currently drag-resizing the widget.
  bool user_resizing_ = false;
  std::unique_ptr<LocalHotkeyManager> application_hotkey_manager_;
  std::unique_ptr<LocalHotkeyManager> glic_panel_hotkey_manager_;
  std::unique_ptr<GlicWindowAnimator> glic_window_animator_;

  // Must outlive `glic_widget_`
  std::unique_ptr<views::WidgetDelegate> glic_delegate_;
  std::unique_ptr<GlicWidget> glic_widget_;
  std::unique_ptr<GlicWindowEventObserver> window_event_observer_;
  mojom::PanelState panel_state_;
  // Observes the glic widget.
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      glic_widget_observation_{this};

  // Used by web modals to listens for glic window events, e.g. size change or
  // window close.
  base::ObserverList<web_modal::ModalDialogHostObserver>::Unchecked
      modal_dialog_host_observers_;

  raw_ptr<Profile> profile_;
  raw_ref<GlicUiEmbedder::Delegate> delegate_;
  raw_ref<GlicInstanceMetrics> instance_metrics_;

  std::unique_ptr<GlicScreenshotCapturer> screenshot_capturer_;

  tabs::TabHandle source_tab_;
  base::CallbackListSubscription source_tab_destruction_subscription_;

  base::WeakPtrFactory<GlicFloatingUi> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_WIDGET_GLIC_FLOATING_UI_H_
