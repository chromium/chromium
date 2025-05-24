// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/app_tab_helper.h"

#include <memory>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "build/build_config.h"
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
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/extension_web_contents_observer.h"
#include "extensions/browser/image_loader.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/icons/extension_icon_set.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "ui/gfx/image/image.h"
#include "url/url_constants.h"

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#endif

using content::WebContents;

namespace extensions {

AppTabHelper::~AppTabHelper() = default;

AppTabHelper::AppTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<AppTabHelper>(*web_contents),
      profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())) {
  // Ensure we have a SessionTabHelper.
  CreateSessionServiceTabHelper(web_contents);

  registry_observation_.Observe(ExtensionRegistry::Get(profile_));
}

void AppTabHelper::SetExtensionApp(const Extension* extension) {
  DCHECK(!extension || AppLaunchInfo::GetFullLaunchURL(extension).is_valid());
  if (extension_app_ == extension) {
    return;
  }

  DCHECK(!extension || extension->is_app());

  extension_app_ = extension;

  UpdateExtensionAppIcon(extension_app_);

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
  if (extension_app_) {
    SessionService* session_service = SessionServiceFactory::GetForProfile(
        Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
    if (session_service) {
      sessions::SessionTabHelper* session_tab_helper =
          sessions::SessionTabHelper::FromWebContents(web_contents());
      CHECK(session_tab_helper);
      session_service->SetTabExtensionAppID(session_tab_helper->window_id(),
                                            session_tab_helper->session_id(),
                                            GetExtensionAppId());
    }
  }
#endif
}

void AppTabHelper::SetExtensionAppById(const ExtensionId& extension_app_id) {
  const Extension* extension = GetExtension(extension_app_id);
  if (extension) {
    SetExtensionApp(extension);
  }
}

ExtensionId AppTabHelper::GetExtensionAppId() const {
  return extension_app_ ? extension_app_->id() : ExtensionId();
}

SkBitmap* AppTabHelper::GetExtensionAppIcon() {
  if (extension_app_icon_.empty()) {
    return nullptr;
  }

  return &extension_app_icon_;
}

void AppTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted() ||
      !navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }

  content::BrowserContext* context = web_contents()->GetBrowserContext();
  ExtensionRegistry* registry = ExtensionRegistry::Get(context);
  const ExtensionSet& enabled_extensions = registry->enabled_extensions();

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
}

void AppTabHelper::DidCloneToNewWebContents(WebContents* old_web_contents,
                                            WebContents* new_web_contents) {
  // When the WebContents that this is attached to is cloned, give the new clone
  // a AppTabHelper and copy state over.
  CreateForWebContents(new_web_contents);
  AppTabHelper* new_helper = FromWebContents(new_web_contents);

  new_helper->SetExtensionApp(extension_app_);
  new_helper->extension_app_icon_ = extension_app_icon_;
}

const Extension* AppTabHelper::GetExtension(
    const ExtensionId& extension_app_id) {
  if (extension_app_id.empty()) {
    return nullptr;
  }

  content::BrowserContext* context = web_contents()->GetBrowserContext();
  return ExtensionRegistry::Get(context)->enabled_extensions().GetByID(
      extension_app_id);
}

void AppTabHelper::UpdateExtensionAppIcon(const Extension* extension) {
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
        base::BindOnce(&AppTabHelper::OnImageLoaded,
                       image_loader_ptr_factory_.GetWeakPtr()));
  }
}

void AppTabHelper::OnImageLoaded(const gfx::Image& image) {
  if (!image.IsEmpty()) {
    extension_app_icon_ = *image.ToSkBitmap();
    web_contents()->NotifyNavigationStateChanged(content::INVALIDATE_TYPE_TAB);
  }
}

void AppTabHelper::OnExtensionUnloaded(content::BrowserContext* browser_context,
                                       const Extension* extension,
                                       UnloadedExtensionReason reason) {
  if (!extension_app_) {
    return;
  }

  if (extension == extension_app_) {
    SetExtensionApp(nullptr);
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AppTabHelper);

}  // namespace extensions
