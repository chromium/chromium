// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ECHE_ECHE_TRAY_H_
#define ASH_SYSTEM_ECHE_ECHE_TRAY_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/public/cpp/keyboard/keyboard_controller_observer.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/shelf/shelf_observer.h"
#include "ash/shell_observer.h"
#include "ash/system/eche/eche_icon_loading_indicator_view.h"
#include "ash/system/screen_layout_observer.h"
#include "ash/system/tray/system_tray_observer.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/webui/eche_app_ui/eche_connection_status_handler.h"
#include "ash/webui/eche_app_ui/mojom/eche_app.mojom-shared.h"
#include "ash/webui/eche_app_ui/mojom/eche_app.mojom.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "ui/display/display_observer.h"
#include "ui/display/tablet_state.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/button/button.h"
#include "url/gurl.h"

namespace display {
enum class TabletState;
}  // namespace display

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
class TrayBubbleView;
class TrayBubbleWrapper;
class SessionControllerImpl;
class Shelf;
class Shell;

// This class represents the Eche tray button in the status area and
// controls the bubble that is shown when the tray button is clicked.
class ASH_EXPORT EcheTray
    : public TrayBackgroundView,
      public SessionObserver,
      public ScreenLayoutObserver,
      public ShelfObserver,
      public SystemTrayObserver,
      public display::DisplayObserver,
      public KeyboardControllerObserver,
      public ShellObserver,
      public eche_app::EcheConnectionStatusHandler::Observer {
  METADATA_HEADER(EcheTray, TrayBackgroundView)

 public:
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
  using BubbleShownCallback = base::RepeatingCallback<void(AshWebView* view)>;

  explicit EcheTray(Shelf* shelf);
  EcheTray(const EcheTray&) = delete;
  EcheTray& operator=(const EcheTray&) = delete;
  ~EcheTray() override;

  bool IsInitialized() const;

  // TrayBackgroundView:
  void ClickedOutsideBubble(const ui::LocatedEvent& event) override;
  void UpdateTrayItemColor(bool is_active) override;
  std::u16string GetAccessibleNameForTray() override;
  void HandleLocaleChange() override;
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override;
  void AnchorUpdated() override;
  void Initialize() override;
  void CloseBubbleInternal() override;
  void ShowBubble() override;
  TrayBubbleView* GetBubbleView() override;
  views::Widget* GetBubbleWidget() const override;
  void OnVirtualKeyboardVisibilityChanged() override;
  bool CacheBubbleViewForHide() const override;

  // TrayBubbleView::Delegate:
  std::u16string GetAccessibleNameForBubble() override;
  bool ShouldEnableExtraKeyboardAccessibility() override;
  void HideBubble(const TrayBubbleView* bubble_view) override;

  // SessionObserver:
  void OnLockStateChanged(bool locked) override;

  // KeyboardControllerObserver:
  void OnKeyboardUIDestroyed() override;
  void OnKeyboardHidden(bool is_temporary_hide) override;

  // eche_app::EcheConnectionStatusHandler::Observer:
  void OnConnectionStatusChanged(
      eche_app::mojom::ConnectionStatus connection_status) override;
  void OnRequestBackgroundConnectionAttempt() override;

  // SystemTrayObserver:
  void OnFocusLeavingSystemTray(bool reverse) override {}
  void OnStatusAreaAnchoredBubbleVisibilityChanged(TrayBubbleView* tray_bubble,
                                                   bool visible) override;

  // Callback called when the eche icon or tray button is pressed.
  void OnButtonPressed();

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

  // Sets a callback that runs when the bubble is shown for the first time, and
  // returns the webview.
  void SetBubbleShownCallback(BubbleShownCallback bubble_shown_callback);

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
                  const std::u16string& phone_name,
                  eche_app::mojom::ConnectionStatus last_connection_status,
                  eche_app::mojom::AppStreamLaunchEntryPoint entry_point);

  // Destroys the view inclusing the web view.
  // Note: `CloseBubble` only hides the view.
  void PurgeAndClose();

  void HideBubble();

  // Receives the `status` change when the video streaming is started or
  // stopped. Controls the bubble widget based on the different `status`
  // changes. There are two cases: 1. Shows the bubble when the streaming is
  // started. 2. Purges and closes the bubble when the streaming is stopped.
  void OnStreamStatusChanged(eche_app::mojom::StreamStatus status);

  // Receives the `orientation` change when the stream switches between
  // landscape and portrait.
  void OnStreamOrientationChanged(bool is_landscape);

  // Set up the params and init the bubble.
  // Note: This function makes the bubble active and makes the
  // TrayBackgroundView's background inkdrop activate.
  void InitBubble(const std::u16string& phone_name,
                  eche_app::mojom::ConnectionStatus last_connection_status,
                  eche_app::mojom::AppStreamLaunchEntryPoint entry_point);

  // Starts graceful close to ensure the connection resource is released before
  // the window is closed.
  void StartGracefulClose();

  void OnBackgroundConnectionTimeout();

  void SetEcheConnectionStatusHandler(
      eche_app::EcheConnectionStatusHandler* eche_connection_status_handler);

  bool IsBackgroundConnectionAttemptInProgress();

  // Test helpers
  bool get_is_landscape_for_test() { return is_landscape_; }
  TrayBubbleWrapper* get_bubble_wrapper_for_test() { return bubble_.get(); }
  AshWebView* get_web_view_for_test() { return web_view_; }
  AshWebView* get_initializer_webview_for_test() {
    return initializer_webview_.get();
  }
  views::ImageButton* GetIcon();

 private:
  FRIEND_TEST_ALL_PREFIXES(EcheTrayTest, EcheTrayCreatesBubbleButHideFirst);
  FRIEND_TEST_ALL_PREFIXES(EcheTrayTest, EcheTrayOnDisplayConfigurationChanged);
  FRIEND_TEST_ALL_PREFIXES(EcheTrayTest,
                           EcheTrayKeyboardShowHideUpdateBubbleBounds);
  FRIEND_TEST_ALL_PREFIXES(EcheTrayTest, EcheTrayOnStreamOrientationChanged);

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
    const raw_ptr<EcheTray> eche_tray_;
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

  // Resize Eche size and update the bubble's position.
  void UpdateEcheSizeAndBubbleBounds();

  // ScreenLayoutObserver:
  void OnDidApplyDisplayChanges() override;

  // ShelfObserver:
  void OnAutoHideStateChanged(ShelfAutoHideState new_state) override;

  // display::DisplayObserver:
  void OnDisplayTabletStateChanged(display::TabletState state) override;

  // ShellObserver:
  void OnShelfAlignmentChanged(aura::Window* root_window,
                               ShelfAlignment old_alignment) override;

  // Called when the display tablet state is changed to kInTabletMode.
  void OnTabletModeStarted();

  // Processes the accelerator keys and returns true if the accelerator was
  // processed completely in this method and no further processing is needed.
  bool ProcessAcceleratorKeys(ui::KeyEvent* event);

  // Returns true only if the bubble is initialized and visible.
  bool IsBubbleVisible();

  // Starts graceful shutdown for the initializer.
  void StartGracefulCloseInitializer();

  // Kills the renderer.
  void CloseInitializer();

  // The url that is transferred to the web view.
  // In the current implementation, this is supposed to be
  // Eche window URL. However, the bubble does not interpret,
  // validate, or expect a special url format or page behabvior.
  GURL url_;

  // Icon of the tray. Unowned.
  const raw_ptr<views::ImageView> icon_;

  // The bubble that appears after clicking the tray button.
  std::unique_ptr<TrayBubbleWrapper> bubble_;

  // The webview shown in the bubble that contains the Eche SWA.
  // owned by `bubble_`
  raw_ptr<AshWebView> web_view_ = nullptr;

  // Webview used to create a prewarming channel, before we have a video to
  // attach to.
  std::unique_ptr<AshWebView> initializer_webview_{};
  std::unique_ptr<base::DelayTimer> initializer_timeout_{};
  base::OnceClosure on_initializer_closed_;
  bool has_reported_initializer_result_ = false;
  bool has_retried_initializer_ = false;

  raw_ptr<eche_app::EcheConnectionStatusHandler>
      eche_connection_status_handler_ = nullptr;

  GracefulCloseCallback graceful_close_callback_;
  GracefulGoBackCallback graceful_go_back_callback_;
  BubbleShownCallback bubble_shown_callback_;

  // The unload timer to force close EcheTray in case unload error.
  std::unique_ptr<base::DelayTimer> unload_timer_;

  raw_ptr<views::View, DanglingUntriaged> header_view_ = nullptr;
  raw_ptr<views::Button> close_button_ = nullptr;
  raw_ptr<views::Button> minimize_button_ = nullptr;
  raw_ptr<views::Button> arrow_back_button_ = nullptr;
  std::unique_ptr<EventInterceptor> event_interceptor_;

  // The time a stream is initializing. Used to record the elapsed time from
  // when the stream is initializing to when the stream is closed by user.
  std::optional<base::TimeTicks> init_stream_timestamp_;

  // The orientation of the stream (portrait vs landscape). The default
  // orientation is portrait.
  bool is_landscape_ = false;

  bool is_stream_started_ = false;
  std::u16string phone_name_;

  // Observers
  base::ScopedObservation<SessionControllerImpl, SessionObserver>
      observed_session_{this};
  base::ScopedObservation<Shelf, ShelfObserver> shelf_observation_{this};
  base::ScopedObservation<Shell, ShellObserver> shell_observer_{this};
  base::ScopedObservation<keyboard::KeyboardUIController,
                          KeyboardControllerObserver>
      keyboard_observation_{this};
  display::ScopedDisplayObserver display_observer_{this};

  base::WeakPtrFactory<EcheTray> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_ECHE_ECHE_TRAY_H_
