// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/tab_helper.h"

#include <memory>

#include "base/check_op.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/activity_log/activity_log.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper_factory.h"
#include "chrome/common/buildflags.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "extensions/browser/api/declarative/rules_registry_service.h"
#include "extensions/browser/api/declarative_content/content_rules_registry.h"
#include "extensions/browser/extension_web_contents_observer.h"
#include "extensions/browser/permissions/active_tab_permission_granter.h"
#include "extensions/browser/permissions/site_permissions_helper.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/manifest.h"
#include "extensions/common/permissions/api_permission.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/extensions/api/bookmark_manager_private/bookmark_manager_private_api.h"
#endif

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#endif

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

using content::WebContents;

namespace extensions {

namespace {

void DisableBackForwardCacheIfNecessary(
    const ExtensionSet& enabled_extensions,
    content::BrowserContext* context,
    content::NavigationHandle* navigation_handle) {
  // User data key for caching if bfcache is disabled.
  static const char kIsBFCacheDisabledKey[] =
      "extensions.backforward.browsercontext";

  if (!content::BackForwardCache::IsBackForwardCacheFeatureEnabled() ||
      context->GetUserData(kIsBFCacheDisabledKey)) {
    return;
  }
}

}  // namespace

TabHelper::~TabHelper() = default;

TabHelper::TabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<TabHelper>(*web_contents),
      profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())),
      script_executor_(std::make_unique<ScriptExecutor>(web_contents)),
      extension_action_runner_(
          std::make_unique<ExtensionActionRunner>(web_contents)),
      declarative_net_request_helper_(web_contents) {
  // The ActiveTabPermissionManager requires a session ID; ensure this
  // WebContents has one.
  CreateSessionServiceTabHelper(web_contents);
  // The [this] is safe because ForEachRenderFrameHost() is synchronous.
  web_contents->ForEachRenderFrameHost(
      [this](content::RenderFrameHost* host) { SetTabId(host); });
  int tab_id = sessions::SessionTabHelper::IdForTab(web_contents).id();
  // TODO(crbug.com/393179880): Pull this creation out of TabHelper once
  // tab id assignment can be done on desktop Android.
  ActiveTabPermissionGranter::CreateForWebContents(web_contents, tab_id,
                                                   profile_);

  ActivityLog::GetInstance(profile_)->ObserveScripts(script_executor_.get());

  InvokeForContentRulesRegistries([this](ContentRulesRegistry* registry) {
    registry->MonitorWebContentsForRuleEvaluation(this->web_contents());
  });

  ExtensionWebContentsObserver::GetForWebContents(web_contents)
      ->dispatcher()
      ->set_delegate(this);

  registry_observation_.Observe(
      ExtensionRegistry::Get(web_contents->GetBrowserContext()));

#if !BUILDFLAG(IS_ANDROID)
  // The Android bookmark manager is native UI, not web UI, so this event router
  // isn't needed on desktop Android.
  BookmarkManagerPrivateDragEventRouter::CreateForWebContents(web_contents);
#endif
}

void TabHelper::SetReloadRequired(
    PermissionsManager::UserSiteSetting site_setting) {
  switch (site_setting) {
    case PermissionsManager::UserSiteSetting::kGrantAllExtensions: {
      // Granting access to all extensions is allowed iff feature is
      // enabled, and it shouldn't be enabled anywhere where this is called.
      NOTREACHED();
    }
    case PermissionsManager::UserSiteSetting::kBlockAllExtensions: {
      // A reload is required if any extension that had site access will lose
      // it.
      content::WebContents* web_contents = GetVisibleWebContents();
      SitePermissionsHelper permissions_helper(profile_);
      const ExtensionSet& extensions =
          ExtensionRegistry::Get(profile_)->enabled_extensions();
      reload_required_ = std::ranges::any_of(
          extensions, [&permissions_helper,
                       web_contents](scoped_refptr<const Extension> extension) {
            return permissions_helper.GetSiteInteraction(*extension,
                                                         web_contents) ==
                   SitePermissionsHelper::SiteInteraction::kGranted;
          });
      break;
    }
    case PermissionsManager::UserSiteSetting::kCustomizeByExtension:
      // When the user selects "customize by extension" it means previously all
      // extensions were blocked and each extension's page access is set as
      // "denied". Blocked actions in the ExtensionActionRunner are computed by
      // checking if a page access is "withheld". Therefore, we always need a
      // refresh since we don't know if there are any extensions that would have
      // wanted to run if the page had not been restricted by the user.
      reload_required_ = true;
      break;
  }
}

bool TabHelper::IsReloadRequired() {
  return reload_required_;
}

void TabHelper::OnWatchedPageChanged(
    const std::vector<std::string>& css_selectors) {
  InvokeForContentRulesRegistries(
      [this, css_selectors](ContentRulesRegistry* registry) {
        registry->OnWatchedPageChanged(web_contents(), css_selectors);
      });
}

// Encapsulates the logic to decide which ContentRulesRegistries need to be
// invoked, depending on whether this WebContents is associated with an Original
// or OffTheRecord profile. In the latter case, we need to invoke on both the
// Original and OffTheRecord ContentRulesRegistries since the Original registry
// handles spanning-mode incognito extensions.
template <class Func>
void TabHelper::InvokeForContentRulesRegistries(const Func& func) {
  RulesRegistryService* rules_registry_service =
      RulesRegistryService::Get(profile_);
  if (rules_registry_service) {
    func(rules_registry_service->content_rules_registry());
    if (profile_->IsOffTheRecord()) {
      // The original profile's content rules registry handles rules for
      // spanning extensions in incognito profiles, so invoke it also.
      RulesRegistryService* original_profile_rules_registry_service =
          RulesRegistryService::Get(profile_->GetOriginalProfile());
      DCHECK_NE(rules_registry_service,
                original_profile_rules_registry_service);
      if (original_profile_rules_registry_service) {
        func(original_profile_rules_registry_service->content_rules_registry());
      }
    }
  }
}

void TabHelper::RenderFrameCreated(content::RenderFrameHost* host) {
  SetTabId(host);
}

void TabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted() ||
      !navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }

  InvokeForContentRulesRegistries(
      [this, navigation_handle](ContentRulesRegistry* registry) {
        registry->DidFinishNavigation(web_contents(), navigation_handle);
      });

  content::BrowserContext* context = web_contents()->GetBrowserContext();
  ExtensionRegistry* registry = ExtensionRegistry::Get(context);
  const ExtensionSet& enabled_extensions = registry->enabled_extensions();

  DisableBackForwardCacheIfNecessary(enabled_extensions, context,
                                     navigation_handle);

  // Reset the `reload_required_` data member, since a page navigation acts as a
  // page refresh.
  reload_required_ = false;
}

void TabHelper::DidCloneToNewWebContents(WebContents* old_web_contents,
                                         WebContents* new_web_contents) {
  // When the WebContents that this is attached to is cloned, give the new clone
  // a TabHelper.
  // TODO(jamescook): Do we still need to do this if we're not copying any
  // state?
  CreateForWebContents(new_web_contents);
}

void TabHelper::WebContentsDestroyed() {
  InvokeForContentRulesRegistries([this](ContentRulesRegistry* registry) {
    registry->WebContentsDestroyed(web_contents());
  });

  reload_required_ = false;
}

WindowController* TabHelper::GetExtensionWindowController() const {
  return ExtensionTabUtil::GetWindowControllerOfTab(web_contents());
}

WebContents* TabHelper::GetAssociatedWebContents() const {
  return web_contents();
}

void TabHelper::OnExtensionLoaded(content::BrowserContext* browser_context,
                                  const Extension* extension) {
  // Clear the back forward cache for the associated tab to accommodate for any
  // side effects of loading/unloading the extension.
  web_contents()->GetController().GetBackForwardCache().Flush();
}

void TabHelper::OnExtensionUnloaded(content::BrowserContext* browser_context,
                                    const Extension* extension,
                                    UnloadedExtensionReason reason) {
  // Clear the back forward cache for the associated tab to accommodate for any
  // side effects of loading/unloading the extension.
  web_contents()->GetController().GetBackForwardCache().Flush();

  // Technically, the refresh is no longer needed if the unloaded extension was
  // the only one causing `refresh_required`. However, we would need to track
  // which are the extensions causing the reload, and sometimes it is not
  // specific to an extensions. Also, this is a very edge case  (site settings
  // changed and then extension is installed externally), so it's fine to not
  // handle it.
}

void TabHelper::SetTabId(content::RenderFrameHost* render_frame_host) {
  // When this is called from the TabHelper constructor during WebContents
  // creation, the renderer-side Frame object would not have been created yet.
  // We should wait for RenderFrameCreated() to happen, to avoid sending this
  // message twice.
  if (render_frame_host->IsRenderFrameLive()) {
    SessionID id = sessions::SessionTabHelper::IdForTab(web_contents());
    CHECK(id.is_valid());
    auto* local_frame =
        ExtensionWebContentsObserver::GetForWebContents(web_contents())
            ->GetLocalFrame(render_frame_host);
    if (!local_frame) {
      return;
    }
    local_frame->SetTabId(id.id());
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(TabHelper);

}  // namespace extensions
