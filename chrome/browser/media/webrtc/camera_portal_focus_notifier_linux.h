// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_CAMERA_PORTAL_FOCUS_NOTIFIER_LINUX_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_CAMERA_PORTAL_FOCUS_NOTIFIER_LINUX_H_

#include <memory>

#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/browser_window/public/browser_collection.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

// Reports a video capture device change the first time a browser window
// receives display-server activation. GNOME Shell refuses the Camera portal
// request while no window of the app is focused, e.g. for a page loaded at
// startup before the compositor activates Chrome's window. The video capture
// service then uses V4L2 and keeps one retry of the portal for the next
// enumeration. The device change makes the browser enumerate cameras again once
// a window has focus.
//
// On Linux, BrowserView::Show() marks the Browser active before the native
// widget is activated by the display server, and the initial startup Browser is
// created before PostBrowserStart(). Observing both the BrowserCollection and
// each browser window's Widget catches the actual native activation of existing
// and newly created windows.
class CameraPortalFocusNotifierLinux : public BrowserCollectionObserver,
                                       public views::WidgetObserver {
 public:
  // Returns a notifier observing `collection`, which must be non-null, when
  // `kWebRtcPipeWireCamera` is enabled, or nullptr otherwise.
  static std::unique_ptr<CameraPortalFocusNotifierLinux> CreateIfEnabled(
      BrowserCollection* collection);

  explicit CameraPortalFocusNotifierLinux(BrowserCollection* collection);

  CameraPortalFocusNotifierLinux(const CameraPortalFocusNotifierLinux&) =
      delete;
  CameraPortalFocusNotifierLinux& operator=(
      const CameraPortalFocusNotifierLinux&) = delete;

  ~CameraPortalFocusNotifierLinux() override;

  // BrowserCollectionObserver:
  void OnBrowserCreated(BrowserWindowInterface* browser) override;
  void OnBrowserActivated(BrowserWindowInterface* browser) override;

  // views::WidgetObserver:
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  void OnWidgetDestroying(views::Widget* widget) override;

 private:
  void ObserveBrowser(BrowserWindowInterface* browser);
  void NotifyAndStopObserving();

  base::ScopedObservation<BrowserCollection, BrowserCollectionObserver>
      collection_observation_{this};
  base::ScopedMultiSourceObservation<views::Widget, views::WidgetObserver>
      widget_observations_{this};
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_CAMERA_PORTAL_FOCUS_NOTIFIER_LINUX_H_
