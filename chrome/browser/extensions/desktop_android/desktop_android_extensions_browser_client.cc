// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "chrome/browser/extensions/api/management/chrome_management_api_delegate.h"
#include "chrome/browser/extensions/api/storage/managed_value_store_cache.h"
#include "chrome/browser/extensions/api/storage/sync_value_store_cache.h"
#include "chrome/browser/extensions/chrome_extensions_browser_client.h"
#include "chrome/browser/extensions/desktop_android/desktop_android_extension_host_delegate.h"
#include "chrome/browser/extensions/error_console/error_console.h"
#include "chrome/browser/ui/webui/devtools/devtools_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/value_store/value_store_factory.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/messaging/messaging_delegate.h"
#include "extensions/browser/api/web_request/web_request_info.h"
#include "extensions/browser/api/web_request/web_request_resource_type.h"
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

class DesktopAndroidExtensionsAPIClient : public ExtensionsAPIClient {
 public:
  DesktopAndroidExtensionsAPIClient() = default;
  ~DesktopAndroidExtensionsAPIClient() override = default;

  // ExtensionsAPIClient:
  MessagingDelegate* GetMessagingDelegate() override {
    // The default implementation does nothing, which is fine for now, since
    // this is mostly needed for:
    //   a) tab-specifics,
    //   b) platform apps, and
    //   c) native messaging
    if (!messaging_delegate_) {
      messaging_delegate_ = std::make_unique<MessagingDelegate>();
    }
    return messaging_delegate_.get();
  }

  ManagementAPIDelegate* CreateManagementAPIDelegate() const override {
    // `ManagementAPI` owns the object.
    return new ChromeManagementAPIDelegate;
  }

  // The following code is used to support chrome.storage api for sync
  // and managed mode, until ChromeExtensionAPIClient is ported for
  // desktop android.
  void AddAdditionalValueStoreCaches(
      content::BrowserContext* context,
      const scoped_refptr<value_store::ValueStoreFactory>& factory,
      SettingsChangedCallback observer,
      std::map<settings_namespace::Namespace,
               raw_ptr<ValueStoreCache, CtnExperimental>>* caches) override {
    // Add support for chrome.storage.sync.
    (*caches)[settings_namespace::SYNC] =
        new SyncValueStoreCache(factory, observer, context->GetPath());

    // Add support for chrome.storage.managed.
    (*caches)[settings_namespace::MANAGED] = new ManagedValueStoreCache(
        *Profile::FromBrowserContext(context), factory, observer);
  }

  // The following code is used to support chrome.webRequest api for
  // CalculateOnHeadersReceivedDelta(), until ChromeExtensionAPIClient is ported
  // for desktop android.
  bool ShouldHideResponseHeader(const GURL& url,
                                const std::string& header_name) const override {
    // Gaia may send a OAUth2 authorization code in the Dice response header,
    // which could allow an extension to generate a refresh token for the
    // account.
    return url.host_piece() ==
               GaiaUrls::GetInstance()->gaia_url().host_piece() &&
           base::CompareCaseInsensitiveASCII(header_name,
                                             signin::kDiceResponseHeader) == 0;
  }

  // The following code (duplicated from ChromeExtensionAPIClient) is used to
  // support chrome.webRequest api where webRequestPermissions calls
  // PermissionHelper::ShouldHideBrowserNetworkRequest(), until
  // ChromeExtensionAPIClient is ported for desktop android.
  bool ShouldHideBrowserNetworkRequest(
      content::BrowserContext* context,
      const WebRequestInfo& request) const override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    // Note: browser initiated non-navigation requests are hidden from
    // extensions. But we do still need to protect some sensitive sub-frame
    // navigation requests. Exclude main frame navigation requests.
    bool is_browser_request =
        request.render_process_id == -1 &&
        request.web_request_type != WebRequestResourceType::MAIN_FRAME;

    // Hide requests made by the Devtools frontend.
    bool is_sensitive_request =
        is_browser_request && DevToolsUI::IsFrontendResourceURL(request.url);

    // Hide requests made by the browser on behalf of the NTP.
    is_sensitive_request |=
        is_browser_request &&
        request.initiator ==
            url::Origin::Create(GURL(chrome::kChromeUINewTabURL));

    // Hide requests made by the browser on behalf of the 1P WebUI NTP.
    is_sensitive_request |=
        is_browser_request &&
        request.initiator ==
            url::Origin::Create(GURL(chrome::kChromeUINewTabPageURL));

    return is_sensitive_request;
  }

 private:
  std::unique_ptr<MessagingDelegate> messaging_delegate_;
};

}  // namespace

void ChromeExtensionsBrowserClient::Init() {
  kiosk_delegate_ = std::make_unique<DesktopAndroidKioskDelegate>();
  api_client_ = std::make_unique<DesktopAndroidExtensionsAPIClient>();
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

const ComponentExtensionResourceManager*
ChromeExtensionsBrowserClient::GetComponentExtensionResourceManager() {
  return nullptr;
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
