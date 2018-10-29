// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SHIM_APP_SHIM_HANDLER_MAC_H_
#define CHROME_BROWSER_APPS_APP_SHIM_APP_SHIM_HANDLER_MAC_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "chrome/common/mac/app_shim_launch.h"

namespace views {
class BridgeFactoryHost;
}  // namespace views

namespace apps {

// Registrar, and interface for services that can handle interactions with OSX
// shim processes.
class AppShimHandler {
 public:
  class Host {
   public:
    // Invoked when the app is successfully launched.
    virtual void OnAppLaunchComplete(AppShimLaunchResult result) = 0;
    // Invoked when the app is closed in the browser process.
    virtual void OnAppClosed() = 0;
    // Invoked when the app should be hidden.
    virtual void OnAppHide() = 0;
    // Invoked when a window becomes visible while the app is hidden. Ensures
    // the shim's "Hide/Show" state is updated correctly and the app can be
    // re-hidden.
    virtual void OnAppUnhideWithoutActivation() = 0;
    // Invoked when the app is requesting user attention.
    virtual void OnAppRequestUserAttention(AppShimAttentionType type) = 0;

    // Allows the handler to determine which app this host corresponds to.
    virtual base::FilePath GetProfilePath() const = 0;
    virtual std::string GetAppId() const = 0;
    virtual views::BridgeFactoryHost* GetViewsBridgeFactoryHost() const = 0;

   protected:
    virtual ~Host() {}
  };

  // Register a handler for an |app_mode_id|.
  static void RegisterHandler(const std::string& app_mode_id,
                              AppShimHandler* handler);

  // Remove a handler for an |app_mode_id|.
  static void RemoveHandler(const std::string& app_mode_id);

  // Returns the handler registered for the given |app_mode_id|. If there is
  // none registered, it returns the default handler or NULL if there is no
  // default handler.
  static AppShimHandler* GetForAppMode(const std::string& app_mode_id);

  // Sets the default handler to return when there is no app-specific handler.
  // Setting this to NULL removes the default handler.
  static void SetDefaultHandler(AppShimHandler* handler);

  // Terminate Chrome if a browser window has never been opened, there are no
  // shell windows, and the app list is not visible.
  static void MaybeTerminate();

  // Whether browser sessions should be restored right now. This is true if
  // the browser has been quit but kept alive because Chrome Apps are still
  // running.
  static bool ShouldRestoreSession();

  // Invoked by the shim host when the shim process is launched. The handler
  // must call OnAppLaunchComplete to inform the shim of the result.
  // |launch_type| indicates the type of launch.
  // |files|, if non-empty, holds an array of files paths given as arguments, or
  // dragged onto the app bundle or dock icon.
  virtual void OnShimLaunch(Host* host,
                            AppShimLaunchType launch_type,
                            const std::vector<base::FilePath>& files) = 0;

  // Invoked by the shim host when the connection to the shim process is closed.
  virtual void OnShimClose(Host* host) = 0;

  // Invoked by the shim host when the shim process receives a focus event.
  // |files|, if non-empty, holds an array of files dragged onto the app bundle
  // or dock icon.
  virtual void OnShimFocus(Host* host,
                           AppShimFocusType focus_type,
                           const std::vector<base::FilePath>& files) = 0;

  // Invoked by the shim host when the shim process is hidden or shown.
  virtual void OnShimSetHidden(Host* host, bool hidden) = 0;

  // Invoked by the shim host when the shim process receives a quit event.
  virtual void OnShimQuit(Host* host) = 0;

 protected:
  AppShimHandler() {}
  virtual ~AppShimHandler() {}
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SHIM_APP_SHIM_HANDLER_MAC_H_
