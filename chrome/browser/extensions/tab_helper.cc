// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/tab_helper.h"

#include <memory>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/activity_log/activity_log.h"
#include "chrome/browser/extensions/api/bookmark_manager_private/bookmark_manager_private_api.h"
#include "chrome/browser/extensions/api/declarative_content/chrome_content_rules_registry.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/install_observer.h"
#include "chrome/browser/extensions/install_tracker.h"
#include "chrome/browser/extensions/install_tracker_factory.h"
#include "chrome/browser/extensions/permissions/site_permissions_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper_factory.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/url_constants.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "extensions/browser/api/declarative/rules_registry_service.h"
#include "extensions/browser/api/declarative_net_request/web_contents_helper.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/extension_web_contents_observer.h"
#include "extensions/browser/image_loader.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/icons/extension_icon_set.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/permissions/api_permission.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "url/url_constants.h"

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#endif

using content::NavigationController;
using content::NavigationEntry;
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
      extension_app_(nullptr),
      script_executor_(new ScriptExecutor(web_contents)),
      extension_action_runner_(new ExtensionActionRunner(web_contents)),
      declarative_net_request_helper_(web_contents) {
  // The ActiveTabPermissionManager requires a session ID; ensure this
  // WebContents has one.
  CreateSessionServiceTabHelper(web_contents);
  // The Unretained() is safe because ForEachRenderFrameHost() is synchronous.
  web_contents->ForEachRenderFrameHost(
      [this](content::RenderFrameHost* host) { SetTabId(host); });
  active_tab_permission_granter_ = std::make_unique<ActiveTabPermissionGranter>(
      web_contents, sessions::SessionTabHelper::IdForTab(web_contents).id(),
      profile_);

  ActivityLog::GetInstance(profile_)->ObserveScripts(script_executor_.get());

  InvokeForContentRulesRegistries([this](ContentRulesRegistry* registry) {
    registry->MonitorWebContentsForRuleEvaluation(this->web_contents());
  });

  ExtensionWebContentsObserver::GetForWebContents(web_contents)->dispatcher()->
      set_delegate(this);

  registry_observation_.Observe(
      ExtensionRegistry::Get(web_contents->GetBrowserContext()));

  BookmarkManagerPrivateDragEventRouter::CreateForWebContents(web_contents);
}

void TabHelper::SetExtensionApp(const Extension* extension) {
  DCHECK(!extension || AppLaunchInfo::GetFullLaunchURL(extension).is_valid());
  if (extension_app_ == extension) {
    return;
  }

  if (extension) {
    DCHECK(extension->is_app());
  }
  extension_app_ = extension;

  UpdateExtensionAppIcon(extension_app_);

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
  if (extension_app_) {
    SessionService* session_service = SessionServiceFactory::GetForProfile(
        Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
    if (session_service) {
      sessions::SessionTabHelper* session_tab_helper =
          sessions::SessionTabHelper::FromWebContents(web_contents());
      session_service->SetTabExtensionAppID(session_tab_helper->window_id(),
                                            session_tab_helper->session_id(),
                                            GetExtensionAppId());
    }
  }
#endif
}

void TabHelper::SetExtensionAppById(const ExtensionId& extension_app_id) {
  const Extension* extension = GetExtension(extension_app_id);
  if (extension)
    SetExtensionApp(extension);
}

ExtensionId TabHelper::GetExtensionAppId() const {
  return extension_app_ ? extension_app_->id() : ExtensionId();
}

SkBitmap* TabHelper::GetExtensionAppIcon() {
  if (extension_app_icon_.empty())
    return nullptr;

  return &extension_app_icon_;
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
      reload_required_ = base::ranges::any_of(
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
      if (original_profile_rules_registry_service)
        func(original_profile_rules_registry_service->content_rules_registry());
    }
  }
}

void TabHelper::RenderFrameCreated(content::RenderFrameHost* host) {
  SetTabId(host);
}

void TabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted() ||
      !navigation_handle->IsInPrimaryMainFrame())
    return;

  InvokeForContentRulesRegistries(
      [this, navigation_handle](ContentRulesRegistry* registry) {
    registry->DidFinishNavigation(web_contents(), navigation_handle);
  });

  content::BrowserContext* context = web_contents()->GetBrowserContext();
  ExtensionRegistry* registry = ExtensionRegistry::Get(context);
  const ExtensionSet& enabled_extensions = registry->enabled_extensions();

  DisableBackForwardCacheIfNecessary(enabled_extensions, context,
                                     navigation_handle);

  Browser* browser = chrome::FindBrowserWithTab(web_contents());
  if (browser && (browser->is_type_app() || browser->is_type_app_popup())) {
    const Extension* extension = registry->GetInstalledExtension(
        web_app::GetAppIdFromApplicationName(browser->app_name()));
    if (extension && AppLaunchInfo::GetFullLaunchURL(extension).is_valid()) {
      DCHECK(extension->is_app());
      SetExtensionApp(extension);
    }
  } else {
    UpdateExtensionAppIcon(
        enabled_extensions.GetExtensionOrAppByURL(navigation_handle->GetURL()));
  }

  // Reset the `reload_required_` data member, since a page navigation acts as a
  // page refresh.
  reload_required_ = false;
}

void TabHelper::DidCloneToNewWebContents(WebContents* old_web_contents,
                                         WebContents* new_web_contents) {
  // When the WebContents that this is attached to is cloned, give the new clone
  // a TabHelper and copy state over.
  CreateForWebContents(new_web_contents);
  TabHelper* new_helper = FromWebContents(new_web_contents);

  new_helper->SetExtensionApp(extension_app_);
  new_helper->extension_app_icon_ = extension_app_icon_;
}

void TabHelper::WebContentsDestroyed() {
  InvokeForContentRulesRegistries([this](ContentRulesRegistry* registry) {
    registry->WebContentsDestroyed(web_contents());
  });

  reload_required_ = false;
}

const Extension* TabHelper::GetExtension(const ExtensionId& extension_app_id) {
  if (extension_app_id.empty())
    return nullptr;

  content::BrowserContext* context = web_contents()->GetBrowserContext();
  return ExtensionRegistry::Get(context)->enabled_extensions().GetByID(
      extension_app_id);
}

void TabHelper::UpdateExtensionAppIcon(const Extension* extension) {
  extension_app_icon_.reset();
  // Ensure previously enqueued callbacks are ignored.
  image_loader_ptr_factory_.InvalidateWeakPtrs();

  // Enqueue OnImageLoaded callback.
  if (extension) {
    ImageLoader* loader = ImageLoader::Get(profile_);
    loader->LoadImageAsync(
        extension,
        IconsInfo::GetIconResource(extension,
                                   extension_misc::EXTENSION_ICON_SMALL,
                                   ExtensionIconSet::Match::kBigger),
        gfx::Size(extension_misc::EXTENSION_ICON_SMALL,
                  extension_misc::EXTENSION_ICON_SMALL),
        base::BindOnce(&TabHelper::OnImageLoaded,
                       image_loader_ptr_factory_.GetWeakPtr()));
  }
}

void TabHelper::OnImageLoaded(const gfx::Image& image) {
  if (!image.IsEmpty()) {
    extension_app_icon_ = *image.ToSkBitmap();
    web_contents()->NotifyNavigationStateChanged(content::INVALIDATE_TYPE_TAB);
  }
}

WindowController* TabHelper::GetExtensionWindowController() const  {
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

  if (!extension_app_)
    return;
  if (extension == extension_app_)
    SetExtensionApp(nullptr);

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
