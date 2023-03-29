// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/tab_helper.h"

#include <memory>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
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
#include "components/back_forward_cache/back_forward_cache_disable.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
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
#include "extensions/common/extension.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/permissions/api_permission.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/features.h"
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

bool AreAllExtensionsAllowedForBFCache() {
  static base::FeatureParam<bool> all_extensions_allowed(
      &features::kBackForwardCache, "all_extensions_allowed", true);
  return all_extensions_allowed.Get();
}

std::string BlockedExtensionListForBFCache() {
  static base::FeatureParam<std::string> extensions_blocked(
      &features::kBackForwardCache, "blocked_extensions", "");
  return extensions_blocked.Get();
}

// Check `enabled_extensions` if any of them are specified in the
// `blocked_extensions` or not.
bool ProcessDisabledExtensions(const std::string& feature,
                               const ExtensionSet& enabled_extensions,
                               content::BrowserContext* context,
                               bool all_allowed,
                               const std::string& blocked_extensions) {
  // If we allow all extensions and there aren't any blocked, then just return.
  if (all_allowed && blocked_extensions.empty())
    return false;

  std::vector<std::string> blocked_extensions_list =
      base::SplitString(blocked_extensions, ",", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);

  // Compute whether we need to disable it.
  bool disabled_feature = false;
  for (const auto& extension : enabled_extensions) {
    // Skip component extensions, apps, themes, shared modules and the google
    // docs pre-installed extension.
    if (Manifest::IsComponentLocation(extension->location()) ||
        extension->is_app() || extension->is_theme() ||
        extension->is_shared_module() ||
        extension->id() == extension_misc::kDocsOfflineExtensionId) {
      continue;
    }
    if (util::IsExtensionVisibleToContext(*extension, context)) {
      // If we are allowing all extensions with a block filter set, and this
      // extension is not in it then continue.
      if (all_allowed &&
          !base::Contains(blocked_extensions_list, extension->id())) {
        continue;
      }

      VLOG(1) << "Disabled " << feature << " due to " << extension->short_name()
              << "," << extension->id();
      disabled_feature = true;
      // TODO(dtapuska): Early termination disabled for now to capture VLOG(1)
      // break;
    }
  }

  return disabled_feature;
}

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

  if (ProcessDisabledExtensions("bfcache", enabled_extensions, context,
                                AreAllExtensionsAllowedForBFCache(),
                                BlockedExtensionListForBFCache())) {
    // Set a user data key indicating we've disabled bfcache for this
    // context.
    context->SetUserData(kIsBFCacheDisabledKey,
                         std::make_unique<base::SupportsUserData::Data>());

    // We do not care if GetPreviousRenderFrameHostId returns a reused
    // RenderFrameHost since disabling the cache multiple times has no side
    // effects.
    content::BackForwardCache::DisableForRenderFrameHost(
        navigation_handle->GetPreviousRenderFrameHostId(),
        back_forward_cache::DisabledReason(
            back_forward_cache::DisabledReasonId::kExtensions));
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

  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
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
}

bool TabHelper::OnMessageReceived(const IPC::Message& message,
                                  content::RenderFrameHost* sender) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(TabHelper, message, sender)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_ContentScriptsExecuting,
                        OnContentScriptsExecuting)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
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
}

void TabHelper::OnContentScriptsExecuting(
    content::RenderFrameHost* host,
    const ExecutingScriptsMap& executing_scripts_map,
    const GURL& on_url) {
  ActivityLog::GetInstance(profile_)->OnScriptsExecuted(
      web_contents(), executing_scripts_map, on_url);
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
                                   ExtensionIconSet::MATCH_BIGGER),
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
}

void TabHelper::SetTabId(content::RenderFrameHost* render_frame_host) {
  // When this is called from the TabHelper constructor during WebContents
  // creation, the renderer-side Frame object would not have been created yet.
  // We should wait for RenderFrameCreated() to happen, to avoid sending this
  // message twice.
  if (render_frame_host->IsRenderFrameLive()) {
    SessionID id = sessions::SessionTabHelper::IdForTab(web_contents());
    CHECK(id.is_valid());
    ExtensionWebContentsObserver::GetForWebContents(web_contents())
        ->GetLocalFrame(render_frame_host)
        ->SetTabId(id.id());
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(TabHelper);

}  // namespace extensions
