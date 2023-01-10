// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ECHE_ECHE_TRAY_H_
#define ASH_SYSTEM_ECHE_ECHE_TRAY_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/public/cpp/keyboard/keyboard_controller_observer.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/shelf/shelf_observer.h"
#include "ash/shell_observer.h"
#include "ash/system/eche/eche_icon_loading_indicator_view.h"
#include "ash/system/screen_layout_observer.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/webui/eche_app_ui/mojom/eche_app.mojom.h"
#include "base/gtest_prod_util.h"
#include "base/timer/timer.h"
#include "ui/events/event_handler.h"
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
class KeyEvent;
}  // namespace ui

namespace gfx {
class Image;
class Size;
}  // namespace gfx

namespace keyboard {
class KeyboardUIController;
}  // namespace keyboard

namespace ash {

class AshWebView;
class PhoneHubTray;
class TabletModeController;
class TrayBubbleView;
class TrayBubbleWrapper;
class SessionControllerImpl;
class Shelf;
class Shell;

// This class represents the Eche tray button in the status area and
// controls the bubble that is shown when the tray button is clicked.
class ASH_EXPORT EcheTray : public TrayBackgroundView,
                            public SessionObserver,
                            public ScreenLayoutObserver,
                            public ShelfObserver,
                            public TabletModeObserver,
                            public KeyboardControllerObserver,
                            public ShellObserver {
 public:
  METADATA_HEADER(EcheTray);

  // TODO(b/226687249): Move to ash/webui/eche_app_ui if dependency cycle error
  // is fixed. Enum representing the connection fail reason. These values are
  // persisted to logs. Entries should not be renumbered and numeric values
  // should never be reused.
  enum class ConnectionFailReason {
    // Initial state.
    kUnknown = 0,

    // Timeout because signaling no response, we don't received any response
    // or request before timeout. Report this from EcheSignaler.
    kSignalingNotTriggered = 1,

    // Timeout because signaling response is late. Report this from
    // EcheSignaler.
    kSignalingHasLateResponse = 2,

    // Timeout because we can't finish the whole connection process on time
    // after receiving the signaling request from the remote device. Report
    // this from EcheSignaler.
    kSignalingHasLateRequest = 3,

    // Timeout because the security channel disconnected. Report this from
    // EcheSignaler.
    kSecurityChannelDisconnected = 4,

    // Connection fail because the device is in the tablet mode. Report this
    // from EcheTray.
    kConnectionFailInTabletMode = 5,

    // Connection fail because the devices are on different networks. Report
    // this from EcheTray.
    kConnectionFailSsidDifferent = 6,

    // Connection fail because the remote device is on cellular network. Report
    // this from EcheTray.
    kConnectionFailRemoteDeviceOnCellular = 7,

    kMaxValue = kConnectionFailRemoteDeviceOnCellular,
  };

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
  TrayBubbleView* GetBubbleView() override;
  views::Widget* GetBubbleWidget() const override;
  void OnVirtualKeyboardVisibilityChanged() override;
  void OnAnyBubbleVisibilityChanged(views::Widget* bubble_widget,
                                    bool visible) override;
  bool CacheBubbleViewForHide() const override;
  void OnThemeChanged() override;

  // TrayBubbleView::Delegate:
  std::u16string GetAccessibleNameForBubble() override;
  bool ShouldEnableExtraKeyboardAccessibility() override;
  void HideBubble(const TrayBubbleView* bubble_view) override;

  // SessionObserver:
  void OnLockStateChanged(bool locked) override;

  // KeyboardControllerObserver:
  void OnKeyboardUIDestroyed() override;
  void OnKeyboardHidden(bool is_temporary_hide) override;

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
  //
  // Returns true if the bubble is loaded or initialized successfully.
  bool LoadBubble(const GURL& url,
                  const gfx::Image& icon,
                  const std::u16string& visible_name,
                  const std::u16string& phone_name);

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
  void InitBubble(const std::u16string& phone_name);

  // Starts graceful close to ensure the connection resource is released before
  // the window is closed.
  void StartGracefulClose();

  // Test helpers
  TrayBubbleWrapper* get_bubble_wrapper_for_test() { return bubble_.get(); }
  AshWebView* get_web_view_for_test() { return web_view_; }
  views::ImageButton* GetIcon();

 private:
  FRIEND_TEST_ALL_PREFIXES(EcheTrayTest, EcheTrayCreatesBubbleButHideFirst);
  FRIEND_TEST_ALL_PREFIXES(EcheTrayTest, EcheTrayOnDisplayConfigurationChanged);
  FRIEND_TEST_ALL_PREFIXES(EcheTrayTest,
                           EcheTrayKeyboardShowHideUpdateBubbleBounds);

  // Intercepts all the events targeted to the internal webview in order to
  // process the accelerator keys.
  class EventInterceptor : public ui::EventHandler {
   public:
    explicit EventInterceptor(EcheTray* eche_tray);

    EventInterceptor(const EventInterceptor&) = delete;
    EventInterceptor& operator=(const EventInterceptor&) = delete;

    ~EventInterceptor() override;

    // ui::EventHandler:
    void OnKeyEvent(ui::KeyEvent* event) override;

   private:
    EcheTray* const eche_tray_;
  };

  // Calculates and returns the size of the Exo bubble based on the screen size
  // and orientation.
  gfx::Size CalculateSizeForEche() const;

  // Handles the click on the "back" arrow in the header.
  void OnArrowBackActivated();

  // Creates the header of the bubble that includes a back arrow,
  // close, and minimize buttons.
  std::unique_ptr<views::View> CreateBubbleHeaderView(
      const std::u16string& phone_name);

  void StopLoadingAnimation();
  void StartLoadingAnimation();
  void SetIconVisibility(bool visibility);

  PhoneHubTray* GetPhoneHubTray();
  EcheIconLoadingIndicatorView* GetLoadingIndicator();

  // Refreshes the header buttons, particularly when the theme changes.
  void RefreshHeaderView();

  // Resize Eche size and update the bubble's position.
  void UpdateEcheSizeAndBubbleBounds();

  // ScreenLayoutObserver:
  void OnDisplayConfigurationChanged() override;

  // ShelfObserver:
  void OnAutoHideStateChanged(ShelfAutoHideState new_state) override;

  // TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;

  // ShellObserver:
  void OnShelfAlignmentChanged(aura::Window* root_window,
                               ShelfAlignment old_alignment) override;

  // returns the position of the anchor that bubble needs to be anchored to.
  gfx::Rect GetAnchor();

  // Processes the accelerator keys and returns true if the accelerator was
  // processed completely in this method and no further processing is needed.
  bool ProcessAcceleratorKeys(ui::KeyEvent* event);

  // Returns true only if the bubble is initialized and visible.
  bool IsBubbleVisible();

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

  views::View* header_view_ = nullptr;
  views::Button* close_button_ = nullptr;
  views::Button* minimize_button_ = nullptr;
  views::Button* arrow_back_button_ = nullptr;
  std::unique_ptr<EventInterceptor> event_interceptor_;

  // The time a stream is initializing. Used to record the elapsed time from
  // when the stream is initializing to when the stream is closed by user.
  absl::optional<base::TimeTicks> init_stream_timestamp_;

  bool is_stream_started_ = false;
  std::u16string phone_name_;

  // Observers
  base::ScopedObservation<SessionControllerImpl, SessionObserver>
      observed_session_{this};
  base::ScopedObservation<Shelf, ShelfObserver> shelf_observation_{this};
  base::ScopedObservation<TabletModeController, TabletModeObserver>
      tablet_mode_observation_{this};
  base::ScopedObservation<Shell, ShellObserver> shell_observer_{this};
  base::ScopedObservation<keyboard::KeyboardUIController,
                          KeyboardControllerObserver>
      keyboard_observation_{this};

  base::WeakPtrFactory<EcheTray> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_ECHE_ECHE_TRAY_H_
