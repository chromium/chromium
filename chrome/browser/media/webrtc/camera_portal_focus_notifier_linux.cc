// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/camera_portal_focus_notifier_linux.h"

#include "base/check.h"
#include "base/feature_list.h"
#include "base/system/system_monitor.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "media/capture/capture_switches.h"
#include "ui/base/base_window.h"

// static
std::unique_ptr<CameraPortalFocusNotifierLinux>
CameraPortalFocusNotifierLinux::CreateIfEnabled(BrowserCollection* collection) {
  CHECK(collection);
#if defined(WEBRTC_USE_PIPEWIRE)
  if (base::FeatureList::IsEnabled(features::kWebRtcPipeWireCamera)) {
    return std::make_unique<CameraPortalFocusNotifierLinux>(collection);
  }
#endif
  return nullptr;
}

CameraPortalFocusNotifierLinux::CameraPortalFocusNotifierLinux(
    BrowserCollection* collection) {
  collection_observation_.Observe(collection);
  collection->ForEach([this](BrowserWindowInterface* browser) {
    ObserveBrowser(browser);
    return collection_observation_.IsObserving();
  });
}

CameraPortalFocusNotifierLinux::~CameraPortalFocusNotifierLinux() = default;

void CameraPortalFocusNotifierLinux::OnBrowserCreated(
    BrowserWindowInterface* browser) {
  ObserveBrowser(browser);
}

void CameraPortalFocusNotifierLinux::OnBrowserActivated(
    BrowserWindowInterface* browser) {
  // BrowserView::Show() reports the activation before the display server
  // activates the window. ObserveBrowser() notifies right away if the window
  // is active already, and otherwise waits for the activation of its Widget.
  ObserveBrowser(browser);
}

void CameraPortalFocusNotifierLinux::OnWidgetActivationChanged(
    views::Widget* widget,
    bool active) {
  if (active) {
    NotifyAndStopObserving();
  }
}

void CameraPortalFocusNotifierLinux::OnWidgetDestroying(views::Widget* widget) {
  widget_observations_.RemoveObservation(widget);
}

void CameraPortalFocusNotifierLinux::ObserveBrowser(
    BrowserWindowInterface* browser) {
  if (!browser || !collection_observation_.IsObserving()) {
    return;
  }
  ui::BaseWindow* window = browser->GetWindow();
  if (!window) {
    return;
  }
  if (window->IsActive()) {
    NotifyAndStopObserving();
    return;
  }
  if (views::Widget* widget =
          views::Widget::GetWidgetForNativeWindow(window->GetNativeWindow())) {
    if (!widget_observations_.IsObservingSource(widget)) {
      widget_observations_.AddObservation(widget);
    }
  }
}

void CameraPortalFocusNotifierLinux::NotifyAndStopObserving() {
  // Only the first activation matters: the capture service retries the portal
  // at most once.
  widget_observations_.RemoveAllObservations();
  collection_observation_.Reset();
  if (auto* monitor = base::SystemMonitor::Get()) {
    monitor->ProcessDevicesChanged(base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE);
  }
}
