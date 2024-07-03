// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASH_WEB_VIEW_H_
#define ASH_PUBLIC_CPP_ASH_WEB_VIEW_H_

#include <optional>

#include "ash/public/cpp/ash_public_export.h"
#include "base/observer_list_types.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/view.h"

class GURL;
enum class WindowOpenDisposition;

namespace ash {

// Id to be used to get the wrapped web view using views::View::GetViewByID.
inline constexpr int kAshWebViewChildWebViewId = 41;

// A view which wraps a views::WebView (and associated WebContents) to work
// around dependency restrictions in Ash.
class ASH_PUBLIC_EXPORT AshWebView : public views::View {
  METADATA_HEADER(AshWebView, views::View)

 public:
  // Initialization parameters which dictate how an instance of AshWebView
  // should behave.
  struct InitParams {
    InitParams();
    InitParams(const InitParams&);
    InitParams& operator=(const InitParams&);
    InitParams(InitParams&&);
    InitParams& operator=(InitParams&&);
    ~InitParams();

    // If enabled, AshWebView will automatically resize to the size
    // desired by its embedded WebContents. Note that, if specified, the
    // WebContents will be bounded by |min_size| and |max_size|.
    bool enable_auto_resize = false;
    std::optional<gfx::Size> min_size;
    std::optional<gfx::Size> max_size;

    // If present the corners of the web view will be clipped to the specified
    // radii.
    std::optional<gfx::RoundedCornersF> rounded_corners;

    // If enabled, AshWebView will suppress navigation attempts of its
    // embedded WebContents. When navigation suppression occurs,
    // Observer::DidSuppressNavigation() will be invoked.
    bool suppress_navigation = false;

    // If enabled, AshWebView can be minimized once we received a ash
    // synthesized back event when we're at the bottom of the stack.
    bool minimize_on_back_key = false;

    // If enabled, AshWebView can record media based on the permissions
    // requested from `MediaCaptureDevicesDispatcher`.
    // When disabled, no media recording is allowed. It is set to `false` by
    // default as recording media is a privacy sensitive operation.
    bool can_record_media = false;

    // If enabled, AshWebView fixes its zoom level to 1 (100%) for this
    // AshWebView. This uses zoom level 1 regardless of default zoom level.
    bool fix_zoom_level_to_one = false;

    // Enables AshWebView to hold wake locks, for example, to keep the screen on
    // while playing video. Passed as an param to init WebContents.
    bool enable_wake_locks = true;

    // Used to override the Media Controls source title. Empty strings will
    // trigger default parent behavior.
    std::string source_title;
  };

  // An observer which receives AshWebView events.
  class Observer : public base::CheckedObserver {
   public:
    // Invoked when the embedded WebContents has stopped loading.
    virtual void DidStopLoading() {}

    // Invoked when the embedded WebContents has suppressed navigation.
    virtual void DidSuppressNavigation(const GURL& url,
                                       WindowOpenDisposition disposition,
                                       bool from_user_gesture) {}

    // Invoked when the embedded WebContents' ability to go back has changed.
    virtual void DidChangeCanGoBack(bool can_go_back) {}

    // Invoked when the focused node within the embedded WebContents has
    // changed.
    virtual void DidChangeFocusedNode(const gfx::Rect& node_bounds_in_screen) {}
  };

  ~AshWebView() override;

  // Returns the inner `WebView` to receive the focus. Please note that we
  // do not want to put the focus on the actual `AshWebView` instance as it is
  // invisible.
  virtual views::View* GetInitiallyFocusedView() = 0;

  // Adds/removes the specified |observer|.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Returns the native view associated w/ the underlying WebContents.
  virtual gfx::NativeView GetNativeView() = 0;

  // Invoke to navigate back in the embedded WebContents' navigation stack. If
  // backwards navigation is not possible, return |false|. Otherwise returns
  // |true| to indicate success.
  virtual bool GoBack() = 0;

  // Invoke to navigate the embedded WebContents' to |url|.
  virtual void Navigate(const GURL& url) = 0;

  // See `WebContents::GetVisibleURL()`.
  virtual const GURL& GetVisibleURL() = 0;

  // See `RenderFrameHost::IsErrorDocument()`.
  virtual bool IsErrorDocument() = 0;

  // Sets the specified `corner_radii` to the native view that hosts the webview.
  virtual void SetCornerRadii(const gfx::RoundedCornersF& corner_radii) = 0;

  // Get a request id if there is a media session.
  virtual const base::UnguessableToken& GetMediaSessionRequestId() = 0;

 protected:
  AshWebView();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASH_WEB_VIEW_H_
