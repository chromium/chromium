// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_DESKTOP_ANDROID_DESKTOP_ANDROID_EXTENSIONS_BROWSER_CLIENT_H_
#define CHROME_BROWSER_EXTENSIONS_DESKTOP_ANDROID_DESKTOP_ANDROID_EXTENSIONS_BROWSER_CLIENT_H_

#include "extensions/browser/extensions_browser_client.h"
#include "extensions/buildflags/buildflags.h"

#if !BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
#error "This file is only used for the experimental desktop-android build."
#endif

namespace extensions {

class ExtensionsAPIClient;

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
// TODO(https://crbug.com/356905053): Delete this class once desktop-android
// properly leverages the extension system.
////////////////////////////////////////////////////////////////////////////////
class DesktopAndroidExtensionsBrowserClient : public ExtensionsBrowserClient {
 public:
  DesktopAndroidExtensionsBrowserClient();
  DesktopAndroidExtensionsBrowserClient(
      const DesktopAndroidExtensionsBrowserClient&) = delete;
  DesktopAndroidExtensionsBrowserClient& operator=(
      const DesktopAndroidExtensionsBrowserClient&) = delete;
  ~DesktopAndroidExtensionsBrowserClient() override;

  // ExtensionsBrowserClient overrides:
  bool IsShuttingDown() override;
  bool AreExtensionsDisabled(const base::CommandLine& command_line,
                             content::BrowserContext* context) override;
  bool IsValidContext(void* context) override;
  bool IsSameContext(content::BrowserContext* first,
                     content::BrowserContext* second) override;
  bool HasOffTheRecordContext(content::BrowserContext* context) override;
  content::BrowserContext* GetOffTheRecordContext(
      content::BrowserContext* context) override;
  content::BrowserContext* GetOriginalContext(
      content::BrowserContext* context) override;
  content::BrowserContext* GetContextRedirectedToOriginal(
      content::BrowserContext* context,
      bool force_guest_profile) override;
  content::BrowserContext* GetContextOwnInstance(
      content::BrowserContext* context,
      bool force_guest_profile) override;
  content::BrowserContext* GetContextForOriginalOnly(
      content::BrowserContext* context,
      bool force_guest_profile) override;
  bool AreExtensionsDisabledForContext(
      content::BrowserContext* context) override;
  bool IsGuestSession(content::BrowserContext* context) const override;
  bool IsExtensionIncognitoEnabled(
      const std::string& extension_id,
      content::BrowserContext* context) const override;
  bool CanExtensionCrossIncognito(
      const Extension* extension,
      content::BrowserContext* context) const override;
  base::FilePath GetBundleResourcePath(
      const network::ResourceRequest& request,
      const base::FilePath& extension_resources_path,
      int* resource_id) const override;
  void LoadResourceFromResourceBundle(
      const network::ResourceRequest& request,
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      const base::FilePath& resource_relative_path,
      int resource_id,
      scoped_refptr<net::HttpResponseHeaders> headers,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client) override;
  bool AllowCrossRendererResourceLoad(
      const network::ResourceRequest& request,
      network::mojom::RequestDestination destination,
      ui::PageTransition page_transition,
      int child_id,
      bool is_incognito,
      const Extension* extension,
      const ExtensionSet& extensions,
      const ProcessMap& process_map,
      const GURL& upstream_url) override;
  PrefService* GetPrefServiceForContext(
      content::BrowserContext* context) override;
  void GetEarlyExtensionPrefsObservers(
      content::BrowserContext* context,
      std::vector<EarlyExtensionPrefsObserver*>* observers) const override;
  ProcessManagerDelegate* GetProcessManagerDelegate() const override;
  mojo::PendingRemote<network::mojom::URLLoaderFactory>
  GetControlledFrameEmbedderURLLoader(
      const url::Origin& app_origin,
      content::FrameTreeNodeId frame_tree_node_id,
      content::BrowserContext* browser_context) override;
  std::unique_ptr<ExtensionHostDelegate> CreateExtensionHostDelegate() override;
  bool DidVersionUpdate(content::BrowserContext* context) override;
  void PermitExternalProtocolHandler() override;
  bool IsInDemoMode() override;
  bool IsScreensaverInDemoMode(const std::string& app_id) override;
  bool IsRunningInForcedAppMode() override;
  bool IsAppModeForcedForApp(const ExtensionId& extension_id) override;
  bool IsLoggedInAsPublicAccount() override;
  ExtensionSystemProvider* GetExtensionSystemFactory() override;
  void RegisterBrowserInterfaceBindersForFrame(
      mojo::BinderMapWithContext<content::RenderFrameHost*>* binder_map,
      content::RenderFrameHost* render_frame_host,
      const Extension* extension) const override;
  std::unique_ptr<RuntimeAPIDelegate> CreateRuntimeAPIDelegate(
      content::BrowserContext* context) const override;
  const ComponentExtensionResourceManager*
  GetComponentExtensionResourceManager() override;
  void BroadcastEventToRenderers(
      events::HistogramValue histogram_value,
      const std::string& event_name,
      base::Value::List args,
      bool dispatch_to_off_the_record_profiles) override;
  ExtensionCache* GetExtensionCache() override;
  bool IsBackgroundUpdateAllowed() override;
  bool IsMinBrowserVersionSupported(const std::string& min_version) override;
  void CreateExtensionWebContentsObserver(
      content::WebContents* web_contents) override;
  ExtensionWebContentsObserver* GetExtensionWebContentsObserver(
      content::WebContents* web_contents) override;
  KioskDelegate* GetKioskDelegate() override;
  bool IsLockScreenContext(content::BrowserContext* context) override;
  std::string GetApplicationLocale() override;
  std::string GetUserAgent() const override;

 private:
  std::unique_ptr<ExtensionCache> extension_cache_;
  std::unique_ptr<KioskDelegate> kiosk_delegate_;
  std::unique_ptr<ExtensionsAPIClient> api_client_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_DESKTOP_ANDROID_DESKTOP_ANDROID_EXTENSIONS_BROWSER_CLIENT_H_
