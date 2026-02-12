// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SELECTION_SELECTION_OVERLAY_CONTROLLER_H_
#define CHROME_BROWSER_GLIC_SELECTION_SELECTION_OVERLAY_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/lens/overlay_base_controller.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace glic {

class SelectionOverlayController : public OverlayBaseController {
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

 private:
  void OnScreenshotTaken(const content::CopyFromSurfaceResult& result);

  void SetScreenshot(const SkBitmap& screenshot, SkBitmap rgb_screenshot);

  bool screenshot_available_ = false;
  SkBitmap initial_screenshot_;
  SkBitmap initial_rgb_screenshot_;
  std::optional<std::vector<uint8_t>> encoded_;

  ui::ScopedUnownedUserData<SelectionOverlayController>
      scoped_unowned_user_data_;

  // Must be the last member.
  base::WeakPtrFactory<SelectionOverlayController> weak_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SELECTION_SELECTION_OVERLAY_CONTROLLER_H_
