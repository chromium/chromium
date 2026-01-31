// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEFAULT_BROWSER_DEFAULT_BROWSER_NOTIFICATION_HANDLER_H_
#define CHROME_BROWSER_DEFAULT_BROWSER_DEFAULT_BROWSER_NOTIFICATION_HANDLER_H_

#include <memory>
#include <optional>

#include "base/callback_list.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/default_browser/default_browser_manager.h"

namespace default_browser {

// Listens to default browser state changes and shows a notification
// when Chrome is no longer the default browser.
class DefaultBrowserNotificationHandler {
 public:
  // The unique ID used to identify and manage this system notification.
  static constexpr char kNotificationId[] = "default_browser_changed";

  explicit DefaultBrowserNotificationHandler(DefaultBrowserManager& manager);
  ~DefaultBrowserNotificationHandler();

  DefaultBrowserNotificationHandler(const DefaultBrowserNotificationHandler&) =
      delete;
  DefaultBrowserNotificationHandler& operator=(
      const DefaultBrowserNotificationHandler&) = delete;

  // Called by the notification delegate when the user interacts with the UI.
  void OnNotificationClick(std::optional<int> button_index);
  void OnNotificationClose(bool by_user);

 private:
  // Callback invoked when the DefaultBrowserManager detects a change in the
  // system default browser state.
  void OnDefaultBrowserStateChanged(DefaultBrowserState state);

  // The observer is automatically unregistered when this object is destroyed.
  base::CallbackListSubscription default_browser_change_subscription_;

  // The default browser manager that provides default browser state updates
  // and coordinates shell integration. It outlive this object.
  const raw_ref<DefaultBrowserManager> manager_;

  // It manages the state and metrics for the active notification.
  // It is created when the notification is shown and destroyed when closed.
  std::unique_ptr<DefaultBrowserController> controller_;

  base::WeakPtrFactory<DefaultBrowserNotificationHandler> weak_ptr_factory_{
      this};
};

}  // namespace default_browser

#endif  // CHROME_BROWSER_DEFAULT_BROWSER_DEFAULT_BROWSER_NOTIFICATION_HANDLER_H_
