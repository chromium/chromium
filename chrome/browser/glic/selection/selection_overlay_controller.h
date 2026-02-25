// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SELECTION_SELECTION_OVERLAY_CONTROLLER_H_
#define CHROME_BROWSER_GLIC_SELECTION_SELECTION_OVERLAY_CONTROLLER_H_

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "chrome/browser/glic/selection/selection_overlay.mojom.h"
#include "chrome/browser/ui/lens/overlay_base_controller.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace glic {

class SelectionOverlayController
    : public OverlayBaseController,
      public selection::SelectionOverlayPageHandler {
 public:
  SelectionOverlayController(tabs::TabInterface* tab,
                             PrefService* pref_service);
  ~SelectionOverlayController() override;

  DECLARE_USER_DATA(SelectionOverlayController);

  // A simple utility that gets the the SelectionOverlayController TabFeature
  // set by the embedding tab of a overlay WebUI hosted in
  // `overlay_web_contents`. May return nullptr if no SelectionOverlayController
  // TabFeature is associated with `overlay_web_contents`.
  static SelectionOverlayController* FromOverlayWebContents(
      content::WebContents* overlay_web_contents);

  // A simple utility that gets the the SelectionOverlayController TabFeature
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

  void Show();
  void Hide();

  std::optional<std::vector<uint8_t>>& GetEncodedData() { return encoded_; }

 private:
  void InitializeOverlay();

  // OverlayBaseController overrides:
  void RequestSyncClose(DismissalSource dismissal_source) override;
  void StartScreenshotFlow() override;
  void NotifyOverlayClosing() override;
  bool IsResultsSidePanelShowing() override;
  GURL GetInitialURL() override;
  void NotifyIsOverlayShowing(bool is_showing) override;
  int GetToolResourceId() override;
  ui::ElementIdentifier GetViewContainerId() override;
  SidePanelEntry::PanelType GetSidePanelType() override;
  bool ShouldCloseSidePanel() override;
  bool ShouldShowPreselectionBubble() override;
  bool UseOverlayBlur() override;
  void NotifyPageNavigated() override;
  void NotifyTabForegrounded() override;
  void NotifyTabWillEnterBackground() override;
  bool IsOverlayViewShared() const override;

  // `selection::SelectionOverlayPageHandler`:
  void DismissOverlay(selection::DismissOverlayReason reason) override;
  void AdjustRegion(selection::SelectedRegionPtr target) override;
  void DeleteRegion(const base::UnguessableToken& id) override;

 private:
  void OnScreenshotTaken(const content::CopyFromSurfaceResult& result);

  void SetScreenshot(const SkBitmap& screenshot, SkBitmap rgb_screenshot);

  // Render all the `selected_regions_` on top of `initial_screenshot_`.
  void RenderRegions();

  void Reset();

  // Connections to and from the overlay WebUI. Only valid while
  // `OverlayBaseController::overlay_view_` is showing and the underlying
  // renderer is alive.
  mojo::Receiver<selection::SelectionOverlayPageHandler> receiver_{this};
  mojo::Remote<selection::SelectionOverlayPage> page_;

  // Stateful members. They should be added to Reset().
  bool screenshot_available_ = false;
  SkBitmap initial_screenshot_;
  SkBitmap initial_rgb_screenshot_;
  std::optional<std::vector<uint8_t>> encoded_;
  // Caches the user-selected region. To be renderer on top of
  // `initial_screenshot_`.
  base::flat_map<base::UnguessableToken, selection::SelectedRegionPtr>
      selected_regions_;

  ui::ScopedUnownedUserData<SelectionOverlayController>
      scoped_unowned_user_data_;

  // Must be the last member.
  base::WeakPtrFactory<SelectionOverlayController> weak_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SELECTION_SELECTION_OVERLAY_CONTROLLER_H_
