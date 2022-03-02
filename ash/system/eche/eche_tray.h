// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ECHE_ECHE_TRAY_H_
#define ASH_SYSTEM_ECHE_ECHE_TRAY_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/system/tray/tray_background_view.h"
#include "components/session_manager/session_manager_types.h"
#include "url/gurl.h"

class AccountId;

namespace views {
class ImageView;
class View;
class Widget;
}  // namespace views

namespace ui {
class Event;
}  // namespace ui

namespace gfx {
class Image;
}  // namespace gfx

namespace ash {

class Shelf;
class TrayBubbleView;
class TrayBubbleWrapper;

// This class represents the Eche tray button in the status area and
// controls the bubble that is shown when the tray button is clicked.
class ASH_EXPORT EcheTray : public TrayBackgroundView, public SessionObserver {
 public:
  METADATA_HEADER(EcheTray);

  explicit EcheTray(Shelf* shelf);
  EcheTray(const EcheTray&) = delete;
  EcheTray& operator=(const EcheTray&) = delete;
  ~EcheTray() override;

  // TrayBackgroundView:
  void ClickedOutsideBubble() override;
  std::u16string GetAccessibleNameForTray() override;
  void HandleLocaleChange() override;
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override;
  void AnchorUpdated() override;
  void Initialize() override;
  void CloseBubble() override;
  void ShowBubble() override;
  bool PerformAction(const ui::Event& event) override;
  TrayBubbleView* GetBubbleView() override;
  views::Widget* GetBubbleWidget() const override;
  void OnThemeChanged() override;

  // TrayBubbleView::Delegate:
  std::u16string GetAccessibleNameForBubble() override;
  bool ShouldEnableExtraKeyboardAccessibility() override;
  void HideBubble(const TrayBubbleView* bubble_view) override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;
  void OnActiveUserSessionChanged(const AccountId& account_id) override;

  // Sets the url that will be passed to the webview.
  // Setting a new value will cause the current bubble be destroyed.
  void SetUrl(const GURL& url);

  // Sets the icon that will be used on the tray.
  void SetIcon(const gfx::Image& icon);

  // Destroys the view inclusing the web view.
  // Note: `CloseBubble` only hides the view.
  void PurgeAndClose();

  void HideBubble();

  // Set up the params and init the bubble.
  // Note: This function makes the bubble active and makes the
  // TrayBackgroundView's background inkdrop activate.
  void InitBubble();

  // Test helpers
  TrayBubbleWrapper* get_bubble_wrapper_for_test() { return bubble_.get(); }

 private:
  // Handles the click on the "back" arrow in the header.
  void OnArrowBackActivated();

  // Creates the header of the bubble that includes a back arrow,
  // close, and minimize buttons.
  std::unique_ptr<views::View> CreateBubbleHeaderView();

  // The url that is transferred to the web view.
  // In the current implementation, this is supposed to be
  // Eche window URL. However, the bubble does not interpret,
  // validate, or expect a special url format or page behabvior.
  GURL url_;

  // Icon of the tray. Unowned.
  views::ImageView* const icon_;

  // The bubble that appears after clicking the tray button.
  std::unique_ptr<TrayBubbleWrapper> bubble_;

  base::WeakPtrFactory<EcheTray> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_ECHE_ECHE_TRAY_H_
