// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SELECTION_SELECTION_OVERLAY_CONTROLLER_H_
#define CHROME_BROWSER_GLIC_SELECTION_SELECTION_OVERLAY_CONTROLLER_H_

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "chrome/browser/glic/host/context/glic_page_context_fetcher.h"
#include "chrome/browser/glic/host/glic.mojom-forward.h"
#include "chrome/browser/glic/selection/selection_overlay.mojom.h"
#include "chrome/browser/ui/lens/overlay_base_controller.h"
#include "components/page_content_annotations/content/page_context_fetcher.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"

namespace content {
class WebContents;
}

namespace input {
struct NativeWebKeyboardEvent;
}

namespace glic {

class GlicSharingManager;

class SelectionOverlayController
    : public OverlayBaseController,
      public selection::SelectionOverlayPageHandler {
 public:
  SelectionOverlayController(tabs::TabInterface* tab,
                             PrefService* pref_service);
  ~SelectionOverlayController() override;

  DECLARE_USER_DATA(SelectionOverlayController);

  // A simple utility that gets the SelectionOverlayController TabFeature
  // set by the embedding tab of a overlay WebUI hosted in
  // `overlay_web_contents`. May return nullptr if no SelectionOverlayController
  // TabFeature is associated with `overlay_web_contents`.
  static SelectionOverlayController* FromOverlayWebContents(
      content::WebContents* overlay_web_contents);

  // A simple utility that gets the SelectionOverlayController TabFeature
  // set by the instances of WebContents associated with a tab. May return
  // nullptr if no SelectionOverlayController TabFeature is associated with
  // `tab_web_contents`.
  static SelectionOverlayController* FromTabWebContents(
      content::WebContents* tab_web_contents);

  // This method is used to set up communication between this instance and the
  // overlay WebUI. This is called by the WebUIController when the WebUI is
  // executing javascript and ready to bind.
  void BindOverlay(
      mojo::PendingReceiver<selection::SelectionOverlayPageHandler> receiver,
      mojo::PendingRemote<selection::SelectionOverlayPage> page);

  // Bind the legacy IPC endpoint. See the comment on
  // `capture_region_observer_`.
  void BindCaptureRegionObserver(
      mojo::PendingRemote<mojom::CaptureRegionObserver> observer);
  static void CaptureRegion(
      tabs::TabInterface* tab,
      GlicSharingManager& sharing_manager,
      mojo::PendingRemote<mojom::CaptureRegionObserver> observer,
      mojom::GetTabContextOptionsPtr options);

  void Show(mojom::GetTabContextOptionsPtr options);
  void Close();

  // `selection::SelectionOverlayPageHandler`:
  void DeleteRegion(const base::UnguessableToken& id) override;

 private:
  void WillDiscardContents(tabs::TabInterface* tab,
                           content::WebContents* old_contents,
                           content::WebContents* new_contents);
  void WillDetach(tabs::TabInterface* tab,
                  tabs::TabInterface::DetachReason reason);
  void TabDeactivated(tabs::TabInterface* tab);

  void InitializeOverlay();

  // `content::WebContentsDelegate`:
  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;

  // OverlayBaseController overrides:
  void CloseUI() override;
  void RequestSyncClose(DismissalSource dismissal_source) override;
  void StartScreenshotFlow() override;
  void NotifyOverlayClosing() override;
  bool IsResultsSidePanelShowing() override;
  GURL GetInitialURL() override;
  void NotifyIsOverlayShowing(bool is_showing) override;
  int GetToolResourceId() override;
  ui::ElementIdentifier GetViewContainerId() override;
  bool UsesContentsContainerView() override;
  SidePanelType GetSidePanelType() override;
  bool ShouldCloseSidePanel() override;
  bool ShouldShowPreselectionBubble() override;
  bool UseOverlayBlur() override;
  void NotifyPageNavigated() override;
  void NotifyTabForegrounded() override;
  void NotifyTabWillEnterBackground() override;
  PreselectionUIConfig GetPreselectionBubbleConfig() override;
  bool IsOverlayViewShared() const override;

  // `selection::SelectionOverlayPageHandler`:
  void DismissOverlay(selection::DismissOverlayReason reason) override;
  void AdjustRegion(selection::SelectedRegionPtr target) override;
  void ClosePreselectionBubble() override;
  void AddBackgroundBlur() override;
  void SetLiveBlur(bool enabled) override;

 private:
  void OnScreenshotTaken(const SkBitmap& bitmap);
  void OnScreenshotRedacted(const SkBitmap& bitmap);
  void PageContextReady(
      base::expected<glic::mojom::GetContextResultPtr,
                     page_content_annotations::FetchPageContextErrorDetails>
          fetch_result);

  void SetScreenshot(const SkBitmap& screenshot, SkBitmap rgb_screenshot);

  // Render all the `selected_regions_` on top of `redacted_screenshot_`.
  void RenderRegions();

  void Reset();
  glic::mojom::AdditionalContextPtr CreateAdditionalContext(
      std::vector<std::pair<base::UnguessableToken,
                            glic::mojom::CapturedRegionPtr>> regions);

  // Connections to and from the overlay WebUI. Only valid while
  // `OverlayBaseController::overlay_view_` is showing and the underlying
  // renderer is alive.
  mojo::Receiver<selection::SelectionOverlayPageHandler> receiver_{this};
  mojo::Remote<selection::SelectionOverlayPage> page_;

  // Legacy IPC that's used to signal the WebUI any browser side errors, and
  // used to dismiss the overlay from the WebUI.
  // TODO(b/452032491): Remove this once the old codepath is no longer used.
  mojo::Remote<mojom::CaptureRegionObserver> capture_region_observer_;

  // Stateful members. They should be added to Reset().
  bool screenshot_available_ = false;
  SkBitmap initial_rgb_screenshot_;
  SkBitmap redacted_screenshot_;
  mojom::TabContextPtr tab_context_;
  mojom::GetTabContextOptionsPtr options_;
  // Caches the user-selected region. To be renderer on top of
  // `initial_screenshot_`.
  base::flat_map<base::UnguessableToken, selection::SelectedRegionPtr>
      selected_regions_;

  ui::ScopedUnownedUserData<SelectionOverlayController>
      scoped_unowned_user_data_;

  // Holds subscriptions for TabInterface callbacks.
  std::vector<base::CallbackListSubscription> tab_subscriptions_;

  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;

  // Must be the last member.
  base::WeakPtrFactory<SelectionOverlayController> weak_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SELECTION_SELECTION_OVERLAY_CONTROLLER_H_
