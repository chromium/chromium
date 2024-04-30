// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_TAB_HELPER_H_
#define CHROME_BROWSER_EXTENSIONS_TAB_HELPER_H_

#include <memory>
#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/extensions/permissions/active_tab_permission_granter.h"
#include "chrome/common/extensions/webstore_install_result.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "extensions/browser/api/declarative_net_request/web_contents_helper.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/browser/script_executor.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/stack_frame.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace content {
class RenderFrameHost;
}

namespace gfx {
class Image;
}

namespace extensions {
class ExtensionActionRunner;
class Extension;

// Per-tab extension helper. Also handles non-extension apps.
class TabHelper : public content::WebContentsObserver,
                  public ExtensionFunctionDispatcher::Delegate,
                  public ExtensionRegistryObserver,
                  public content::WebContentsUserData<TabHelper> {
 public:
  TabHelper(const TabHelper&) = delete;
  TabHelper& operator=(const TabHelper&) = delete;

  ~TabHelper() override;

  // Sets the extension denoting this as an app. If |extension| is non-null this
  // tab becomes an app-tab. WebContents does not listen for unload events for
  // the extension. It's up to consumers of WebContents to do that.
  //
  // NOTE: this should only be manipulated before the tab is added to a browser.
  // TODO(sky): resolve if this is the right way to identify an app tab. If it
  // is, then this should be passed in the constructor.
  void SetExtensionApp(const Extension* extension);

  // Convenience for setting the app extension by id. This does nothing if
  // |extension_app_id| is empty, or an extension can't be found given the
  // specified id.
  void SetExtensionAppById(const ExtensionId& extension_app_id);

  // Returns true if an app extension has been set.
  bool is_app() const { return extension_app_ != nullptr; }

  // Return ExtensionId for extension app.
  // If an app extension has not been set, returns empty id.
  ExtensionId GetExtensionAppId() const;

  // If an app extension has been explicitly set for this WebContents its icon
  // is returned.
  //
  // NOTE: the returned icon is larger than 16x16 (its size is
  // extension_misc::EXTENSION_ICON_SMALLISH).
  SkBitmap* GetExtensionAppIcon();

  // Sets whether the tab will require a page reload for applying
  // `site_setting`.
  void SetReloadRequired(PermissionsManager::UserSiteSetting site_setting);

  // Returns whether a page reload is required to apply the user site settings
  // in the tab.
  bool IsReloadRequired();

  ScriptExecutor* script_executor() {
    return script_executor_.get();
  }

  ExtensionActionRunner* extension_action_runner() {
    return extension_action_runner_.get();
  }

  ActiveTabPermissionGranter* active_tab_permission_granter() {
    return active_tab_permission_granter_.get();
  }

  void OnWatchedPageChanged(const std::vector<std::string>& css_selectors);

 private:
  // Utility function to invoke member functions on all relevant
  // ContentRulesRegistries.
  template <class Func>
  void InvokeForContentRulesRegistries(const Func& func);

  explicit TabHelper(content::WebContents* web_contents);

  friend class content::WebContentsUserData<TabHelper>;

  // content::WebContentsObserver overrides.
  void RenderFrameCreated(content::RenderFrameHost* host) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidCloneToNewWebContents(
      content::WebContents* old_web_contents,
      content::WebContents* new_web_contents) override;
  void WebContentsDestroyed() override;

  // ExtensionFunctionDispatcher::Delegate overrides.
  WindowController* GetExtensionWindowController() const override;
  content::WebContents* GetAssociatedWebContents() const override;

  // ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

  // App extensions related methods:

  // Resets app_icon_ and if |extension| is non-null uses ImageLoader to load
  // the extension's image asynchronously.
  void UpdateExtensionAppIcon(const Extension* extension);

  const Extension* GetExtension(const ExtensionId& extension_app_id);

  void OnImageLoaded(const gfx::Image& image);

  // Sends our tab ID to |render_frame_host|.
  void SetTabId(content::RenderFrameHost* render_frame_host);

  raw_ptr<Profile> profile_;

  // If non-null this tab is an app tab and this is the extension the tab was
  // created for.
  raw_ptr<const Extension> extension_app_;

  // Icon for extension_app_ (if non-null) or a manually-set icon for
  // non-extension apps.
  SkBitmap extension_app_icon_;

  std::unique_ptr<ScriptExecutor> script_executor_;

  std::unique_ptr<ExtensionActionRunner> extension_action_runner_;

  declarative_net_request::WebContentsHelper declarative_net_request_helper_;

  std::unique_ptr<ActiveTabPermissionGranter> active_tab_permission_granter_;

  // Whether the tab needs a page reload to apply the user site settings.
  bool reload_required_ = false;

  // Extensions that have dismissed site access requests for this tab's origin.
  std::set<ExtensionId> dismissed_extensions_;

  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      registry_observation_{this};

  // Vend weak pointers that can be invalidated to stop in-progress loads.
  base::WeakPtrFactory<TabHelper> image_loader_ptr_factory_{this};

  // Generic weak ptr factory for posting callbacks.
  base::WeakPtrFactory<TabHelper> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_TAB_HELPER_H_
