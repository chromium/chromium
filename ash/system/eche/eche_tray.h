// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ECHE_ECHE_TRAY_H_
#define ASH_SYSTEM_ECHE_ECHE_TRAY_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/keyboard/keyboard_controller_observer.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_observer.h"
#include "ash/shell.h"
#include "ash/shell_observer.h"
#include "ash/system/eche/eche_icon_loading_indicator_view.h"
#include "ash/system/screen_layout_observer.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/webui/eche_app_ui/mojom/eche_app.mojom.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/gtest_prod_util.h"
#include "base/timer/timer.h"
#include "components/session_manager/session_manager_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/button/button.h"
#include "url/gurl.h"

namespace views {

class ImageView;
class ImageButton;
class View;
class Widget;

}  // namespace views

namespace ui {
class Event;
}  // namespace ui

namespace gfx {
class Image;
class Size;
}  // namespace gfx

namespace ash {

class Shelf;
class TrayBubbleView;
class TrayBubbleWrapper;
class AshWebView;
class PhoneHubTray;

// This class represents the Eche tray button in the status area and
// controls the bubble that is shown when the tray button is clicked.
class ASH_EXPORT EcheTray : public TrayBackgroundView,
                            public SessionObserver,
                            public ScreenLayoutObserver,
                            public ShelfObserver,
                            public TabletModeObserver,
                            public KeyboardControllerObserver,
                            ShellObserver {
 public:
  METADATA_HEADER(EcheTray);

  using GracefulCloseCallback = base::OnceCallback<void()>;
  using GracefulGoBackCallback = base::RepeatingCallback<void()>;

  explicit EcheTray(Shelf* shelf);
  EcheTray(const EcheTray&) = delete;
  EcheTray& operator=(const EcheTray&) = delete;
  ~EcheTray() override;

  bool IsInitialized() const;

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

  // TrayBubbleView::Delegate:
  std::u16string GetAccessibleNameForBubble() override;
  bool ShouldEnableExtraKeyboardAccessibility() override;
  void HideBubble(const TrayBubbleView* bubble_view) override;

  // SessionObserver:
  void OnLockStateChanged(bool locked) override;

  // KeyboardControllerObserver:
  void OnKeyboardUIDestroyed() override;
  void OnKeyboardVisibilityChanged(bool visible) override;

  // Sets the url that will be passed to the webview.
  // Setting a new value will cause the current bubble be destroyed.
  void SetUrl(const GURL& url);

  // Sets the icon that will be used on the tray.
  void SetIcon(const gfx::Image& icon, const std::u16string& tooltip_text);

  // Reduces the size of the original icon by the `offset`. Passing a zero
  // `offset` will bring the icon back to its original size.
  void ResizeIcon(int offset_dip);

  // Sets graceful close callback function. When close Eche Bubble, it will
  // notify to Eche Web to release connection resource.  Be aware that once this
  // is set, close button will not call PurgeAndClose() but rely on Eche Web to
  // close window when connection resource is released; if it is not set, then
  // it will immediately call PurgeAndClose() to close window.
  void SetGracefulCloseCallback(GracefulCloseCallback graceful_close_callback);

  // Sets graceful go back callback function. When users click the ArrowBack
  // button in the Eche Bubble, `graceful_go_back_callback` will notify Eche
  // web content to send the GoBack key event. Be aware that once this is set,
  // the ArrowBack button will call `web_view.GoBack()` and run
  // `graceful_go_back_callback` together and rely on Eche web content to send
  // the GoBack key event to the server when the ArrowBack button is clicked; if
  // this is not set, then the ArrowBack button will immediately call
  // `web_view.GoBack()` to go back the previous page.
  void SetGracefulGoBackCallback(
      GracefulGoBackCallback graceful_go_back_callback);

  views::Button* GetMinimizeButtonForTesting() const;
  views::Button* GetCloseButtonForTesting() const;
  views::Button* GetArrowBackButtonForTesting() const;

  // Initializes the bubble with given parameters. If there is any previous
  // bubble already shown with a different URL it is going to be closed. The
  // bubble is not shown initially until `ShowBubble` is called.
  // The `url` parameter is used to load the `WebView` inside the bubble.
  // The `icon` is used to update the tray icon for `EcheTray`.
  // The `visible_name` is shown as a tooltip for the Eche icon.
  void LoadBubble(const GURL& url,
                  const gfx::Image& icon,
                  const std::u16string& visible_name);

  // Destroys the view inclusing the web view.
  // Note: `CloseBubble` only hides the view.
  void PurgeAndClose();

  void HideBubble();

  // Receives the `status` change when the video streaming is started or
  // stopped. Controls the bubble widget based on the different `status`
  // changes. There are two cases: 1. Shows the bubble when the streaming is
  // started. 2. Purges and closes the bubble when the streaming is stopped.
  void OnStreamStatusChanged(eche_app::mojom::StreamStatus status);

  // Set up the params and init the bubble.
  // Note: This function makes the bubble active and makes the
  // TrayBackgroundView's background inkdrop activate.
  void InitBubble();

  // Test helpers
  TrayBubbleWrapper* get_bubble_wrapper_for_test() { return bubble_.get(); }
  AshWebView* get_web_view_for_test() { return web_view_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(EcheTrayTest, EcheTrayCreatesBubbleButHideFirst);

  // Calculates and returns the size of the Exo bubble based on the screen size
  // and orientation.
  gfx::Size CalculateSizeForEche() const;

  // Handles the click on the "back" arrow in the header.
  void OnArrowBackActivated();

  // Creates the header of the bubble that includes a back arrow,
  // close, and minimize buttons.
  std::unique_ptr<views::View> CreateBubbleHeaderView();

  views::ImageButton* GetIcon();
  void StopLoadingAnimation();
  void StartLoadingAnimation();
  void SetIconVisibility(bool visibility);

  // Starts graceful close to ensure connection resource is released before
  // window is closed.
  void StartGracefulClose();

  PhoneHubTray* GetPhoneHubTray();
  EcheIconLoadingIndicatorView* GetLoadingIndicator();

  // Updates the bubble's position based on the movements of the shelf.
  void UpdateBubbleBounds();

  // ScreenLayoutObserver:
  void OnDisplayConfigurationChanged() override;

  // ShelfObserver:
  void OnAutoHideStateChanged(ShelfAutoHideState new_state) override;
  void OnShelfIconPositionsChanged() override;

  // TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;

  // ShellObserver:
  void OnShelfAlignmentChanged(aura::Window* root_window,
                               ShelfAlignment old_alignment) override;

  // returns the position of the anchor that bubble needs to be anchored to.
  gfx::Rect GetAnchor();

  // The url that is transferred to the web view.
  // In the current implementation, this is supposed to be
  // Eche window URL. However, the bubble does not interpret,
  // validate, or expect a special url format or page behabvior.
  GURL url_;

  // Icon of the tray. Unowned.
  views::ImageView* const icon_;

  // The bubble that appears after clicking the tray button.
  std::unique_ptr<TrayBubbleWrapper> bubble_;

  // The webview shown in the bubble that contains the Eche SWA.
  // owned by `bubble_`
  AshWebView* web_view_ = nullptr;

  GracefulCloseCallback graceful_close_callback_;
  GracefulGoBackCallback graceful_go_back_callback_;

  // The unload timer to force close EcheTray in case unload error.
  std::unique_ptr<base::DelayTimer> unload_timer_;

  views::Button* close_button_ = nullptr;
  views::Button* minimize_button_ = nullptr;
  views::Button* arrow_back_button_ = nullptr;

  // The time a stream is initializing. Used to record the elapsed time from
  // when the stream is initializing to when the stream is closed by user.
  absl::optional<base::TimeTicks> init_stream_timestamp_;

  // Observers
  base::ScopedObservation<SessionControllerImpl, SessionObserver>
      observed_session_{this};
  base::ScopedObservation<Shelf, ShelfObserver> shelf_observation_{this};
  base::ScopedObservation<TabletModeController, TabletModeObserver>
      tablet_mode_observation_{this};
  base::ScopedObservation<Shell,
                          ShellObserver,
                          &Shell::AddShellObserver,
                          &Shell::RemoveShellObserver>
      shell_observer_{this};
  base::ScopedObservation<keyboard::KeyboardUIController,
                          ash::KeyboardControllerObserver>
      keyboard_observation_{this};

  base::WeakPtrFactory<EcheTray> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_ECHE_ECHE_TRAY_H_
