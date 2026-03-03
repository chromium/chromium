// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "build/build_config.h"
#include "chrome/browser/extensions/browser_window_util.h"
#include "chrome/browser/extensions/chrome_extension_host_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extensions_browser_client.h"
#include "ui/base/base_window.h"

static_assert(!BUILDFLAG(IS_ANDROID));

namespace extensions {

void ChromeExtensionHostDelegate::CreateTab(
    std::unique_ptr<content::WebContents> web_contents,
    const ExtensionId& extension_id,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    bool user_gesture) {
  // Verify that the browser is not shutting down. It can be the case if the
  // call is propagated through a posted task that was already in the queue when
  // shutdown started. See crbug.com/40475418
  if (ExtensionsBrowserClient::Get()->IsShuttingDown()) {
    return;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  CHECK(profile);
  BrowserWindowInterface* browser =
      browser_window_util::GetLastActiveNormalBrowserWithProfile(
          *profile, /*include_incognito_or_parent=*/false);

  const bool browser_created = !browser;

  if (!browser) {
    if (Browser::GetCreationStatusForProfile(profile) !=
        Browser::CreationStatus::kOk) {
      // No active browser and can't create a new one. Bail.
      return;
    }

    BrowserWindowCreateParams params(BrowserWindowInterface::TYPE_NORMAL,
                                     *profile, user_gesture);
    // Note: This uses a non-android variant of this method that returns a
    // fully-initialized browser. On android, this would need to be async.
    browser = CreateBrowserWindow(std::move(params));
    // CreateBrowserWindow() should never fail at this point.
    CHECK(browser);
  }

  NavigateParams params(browser, std::move(web_contents));
  // The extension_app_id parameter ends up as app_name in the Browser
  // which causes the Browser to return true for is_app().  This affects
  // among other things, whether the location bar gets displayed.
  // TODO(mpcomplete): This seems wrong. What if the extension content is hosted
  // in a tab?
  if (disposition == WindowOpenDisposition::NEW_POPUP) {
    params.app_id = extension_id;
  }

  params.disposition = disposition;
  params.window_features = window_features;
  params.window_action = NavigateParams::WindowAction::kShowWindow;
  params.user_gesture = user_gesture;
  Navigate(&params);

  // Close the browser if Navigate created a new one.
  if (browser_created && (browser != params.browser)) {
    browser->GetWindow()->Close();
  }
}

}  // namespace extensions
