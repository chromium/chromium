// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TABS_WINDOWS_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_TABS_WINDOWS_API_H_

#include <optional>
#include <string>
#include <vector>

#include "base/types/expected.h"
#include "build/build_config.h"
#include "chrome/common/extensions/api/windows.h"
#include "extensions/browser/extension_function.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#endif

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

class BrowserWindowInterface;
class Profile;
class SessionID;

namespace content {
class WebContents;
}

namespace gfx {
class Rect;
}

namespace extensions {

class WindowController;

class WindowsGetFunction : public ExtensionFunction {
  ~WindowsGetFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("windows.get", WINDOWS_GET)
};
class WindowsGetCurrentFunction : public ExtensionFunction {
  ~WindowsGetCurrentFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("windows.getCurrent", WINDOWS_GETCURRENT)
};
class WindowsGetLastFocusedFunction : public ExtensionFunction {
  ~WindowsGetLastFocusedFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("windows.getLastFocused", WINDOWS_GETLASTFOCUSED)
};
class WindowsGetAllFunction : public ExtensionFunction {
  ~WindowsGetAllFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("windows.getAll", WINDOWS_GETALL)
};
class WindowsCreateFunction : public ExtensionFunction {
 public:
  WindowsCreateFunction();
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("windows.create", WINDOWS_CREATE)

  // Ensures the tab for the window is valid.
  static base::expected<void, std::string> ValidateTab(
      WindowController* source_window,
      Profile* window_profile,
      content::WebContents* web_contents,
      bool is_locked_fullscreen = false);

 private:
  ~WindowsCreateFunction() override;

  // Uses `create_data` to set the window position and size in `window_bounds`.
  // Returns an error string, or the empty string if the bounds are valid.
  static std::string SetWindowBounds(
      const api::windows::Create::Params::CreateData& create_data,
      gfx::Rect& window_bounds);

#if BUILDFLAG(IS_ANDROID)
  void OnBrowserWindowCreatedAsynchronously(BrowserWindowInterface* new_window);
#endif

  // Handles post-creation window initialization. `new_window` is the newly-
  // created browser window.
  // Returns the response to pass back to the extension.
  ResponseValue OnBrowserWindowCreated(BrowserWindowInterface* new_window);

#if BUILDFLAG(IS_CHROMEOS)
  void OnBocaWindowCreatedAsynchronously(const SessionID& session_id);
#endif  // BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_ANDROID)
  // The info for an isolated web app to open, if any.
  std::optional<web_app::IsolatedWebAppUrlInfo> isolated_web_app_url_info_;
#endif

  // The creation data parameters supplied by the extension.
  std::optional<api::windows::Create::Params::CreateData> create_data_;

  // The set of parsed URLs to open in the newly-created window.
  std::vector<GURL> urls_;

  // Whether to set the calling extension context as the opener of the newly-
  // created window. Not supported for service worker callers.
  bool set_self_as_opener_ = false;
};
class WindowsUpdateFunction : public ExtensionFunction {
  ~WindowsUpdateFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("windows.update", WINDOWS_UPDATE)

 private:
  // Applies the updates from `params` to the `browser` window.
  void UpdateWindowState(const api::windows::Update::Params& params,
                         BrowserWindowInterface* browser,
                         WindowController* window_controller,
                         ui::mojom::WindowShowState show_state,
                         bool set_window_bounds,
                         const gfx::Rect& window_bounds);
};
class WindowsRemoveFunction : public ExtensionFunction {
  ~WindowsRemoveFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("windows.remove", WINDOWS_REMOVE)
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_TABS_WINDOWS_API_H_
