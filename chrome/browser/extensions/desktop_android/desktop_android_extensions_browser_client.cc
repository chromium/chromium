// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "chrome/browser/extensions/chrome_extensions_browser_client.h"
#include "chrome/browser/extensions/desktop_android/desktop_android_extension_host_delegate.h"
#include "chrome/browser/extensions/error_console/error_console.h"
#include "chrome/browser/extensions/user_script_listener.h"
#include "chrome/browser/ui/webui/devtools/devtools_ui.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_error.h"
#include "extensions/browser/extension_web_contents_observer.h"
#include "extensions/browser/extensions_browser_interface_binders.h"
#include "extensions/browser/kiosk/kiosk_delegate.h"
#include "extensions/browser/null_app_sorting.h"
#include "extensions/browser/updater/null_extension_cache.h"
#include "extensions/browser/updater/scoped_extension_updater_keep_alive.h"
#include "extensions/browser/url_request_util.h"
#include "extensions/common/features/feature_channel.h"
#include "google_apis/gaia/gaia_urls.h"
#include "services/network/public/mojom/url_loader.mojom.h"

using content::BrowserContext;
using content::BrowserThread;

////////////////////////////////////////////////////////////////////////////////
// S  T  O  P
// ALL THIS CODE WILL BE DELETED.
// THINK TWICE (OR THRICE) BEFORE ADDING MORE.
//
// The details:
// This is part of an experimental desktop-android build and allows us to
// bootstrap the extension system by incorporating a lightweight extensions
// runtime into the chrome binary. This allows us to do things like load
// extensions in tests and exercise code in these builds without needing to have
// the entirety of the //chrome/browser/extensions system either compiled and
// implemented (which is a massive undertaking) or gracefully if-def'd out
// (which is a massive amount of technical debt).
// This approach, by comparison, allows us to have a minimal interface in the
// chrome browser that mostly relies on the top-level //extensions layer, along
// with small bits of the //chrome code that compile cleanly on the
// experimental desktop-android build.
//
// This entire class should go away. Instead of adding new functionality here,
// it should be added in a location that can be shared across desktop-android
// and other desktop builds. In practice, this means:
// * Pulling the code up to //extensions. If it can be cleanly segmented from
//   the //chrome layer, this is preferable. It gets cleanly included across
//   all builds, encourages proper separation of concerns, and reduces the
//   interdependency between features.
// * Including the functionality in the desktop-android build. This can be done
//   for //chrome sources that do not have any dependencies on areas that
//   cannot be included in desktop-android (such as dependencies on `Browser`
//   or native UI code).
//
// TODO(https://crbug.com/356905053): Delete this file once desktop-android
// properly leverages the extension system.
////////////////////////////////////////////////////////////////////////////////

namespace extensions {

namespace {

class DesktopAndroidKioskDelegate : public KioskDelegate {
 public:
  DesktopAndroidKioskDelegate() = default;
  ~DesktopAndroidKioskDelegate() override = default;

  bool IsAutoLaunchedKioskApp(const ExtensionId& id) const override {
    // Desktop-android does not support kiosk apps.
    return false;
  }
};

}  // namespace

void ChromeExtensionsBrowserClient::Init() {
  kiosk_delegate_ = std::make_unique<DesktopAndroidKioskDelegate>();
  // Must occur after g_browser_process is initialized.
  user_script_listener_ = std::make_unique<UserScriptListener>();
}

void ChromeExtensionsBrowserClient::GetEarlyExtensionPrefsObservers(
    content::BrowserContext* context,
    std::vector<EarlyExtensionPrefsObserver*>* observers) const {}

ProcessManagerDelegate*
ChromeExtensionsBrowserClient::GetProcessManagerDelegate() const {
  return nullptr;
}

mojo::PendingRemote<network::mojom::URLLoaderFactory>
ChromeExtensionsBrowserClient::GetControlledFrameEmbedderURLLoader(
    const url::Origin& app_origin,
    content::FrameTreeNodeId frame_tree_node_id,
    content::BrowserContext* browser_context) {
  return mojo::PendingRemote<network::mojom::URLLoaderFactory>();
}

std::unique_ptr<ExtensionHostDelegate>
ChromeExtensionsBrowserClient::CreateExtensionHostDelegate() {
  return std::make_unique<DesktopAndroidExtensionHostDelegate>();
}

void ChromeExtensionsBrowserClient::RegisterBrowserInterfaceBindersForFrame(
    mojo::BinderMapWithContext<content::RenderFrameHost*>* binder_map,
    content::RenderFrameHost* render_frame_host,
    const Extension* extension) const {
  PopulateExtensionFrameBinders(binder_map, render_frame_host, extension);
}

void ChromeExtensionsBrowserClient::ReportError(
    content::BrowserContext* context,
    std::unique_ptr<ExtensionError> error) {
  LOG(ERROR) << error->GetDebugString();
  ErrorConsole::Get(context)->ReportError(std::move(error));
}

KioskDelegate* ChromeExtensionsBrowserClient::GetKioskDelegate() {
  return kiosk_delegate_.get();
}

}  // namespace extensions
