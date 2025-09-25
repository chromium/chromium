// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_TAB_HELPER_H_
#define CHROME_BROWSER_EXTENSIONS_TAB_HELPER_H_

#include <memory>
#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/common/extensions/webstore_install_result.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "extensions/browser/api/declarative_net_request/web_contents_helper.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/browser/script_executor.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/stack_frame.h"
#include "third_party/skia/include/core/SkBitmap.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace content {
class RenderFrameHost;
}

namespace extensions {
class ExtensionActionRunner;
class Extension;

// Per-tab extension helper.
class TabHelper : public content::WebContentsObserver,
                  public ExtensionFunctionDispatcher::Delegate,
                  public ExtensionRegistryObserver,
                  public content::WebContentsUserData<TabHelper> {
 public:
  TabHelper(const TabHelper&) = delete;
  TabHelper& operator=(const TabHelper&) = delete;

  ~TabHelper() override;

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

  // Sends our tab ID to `render_frame_host`.
  void SetTabId(content::RenderFrameHost* render_frame_host);

  raw_ptr<Profile> profile_;

  std::unique_ptr<ScriptExecutor> script_executor_;

  std::unique_ptr<ExtensionActionRunner> extension_action_runner_;

  declarative_net_request::WebContentsHelper declarative_net_request_helper_;

  // Whether the tab needs a page reload to apply the user site settings.
  bool reload_required_ = false;

  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      registry_observation_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_TAB_HELPER_H_
