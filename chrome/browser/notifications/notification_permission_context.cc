// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_permission_context.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/rand_util.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/visibility_timer_tab_helper.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_request_id.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#include "chrome/browser/android/flags/chrome_cached_flags.h"
#include "chrome/browser/android/shortcut_helper.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/webapps/installable/installed_webapp_bridge.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/notifications/notifier_state_tracker.h"
#include "chrome/browser/notifications/notifier_state_tracker_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

// static
void NotificationPermissionContext::UpdatePermission(
    content::BrowserContext* browser_context,
    const GURL& origin,
    ContentSetting setting) {
  switch (setting) {
    case CONTENT_SETTING_ALLOW:
    case CONTENT_SETTING_BLOCK:
    case CONTENT_SETTING_DEFAULT:
      HostContentSettingsMapFactory::GetForProfile(browser_context)
          ->SetContentSettingDefaultScope(
              origin, GURL(), ContentSettingsType::NOTIFICATIONS, setting);
      break;

    default:
      NOTREACHED_IN_MIGRATION();
  }
}

NotificationPermissionContext::NotificationPermissionContext(
    content::BrowserContext* browser_context)
    : PermissionContextBase(browser_context,
                            ContentSettingsType::NOTIFICATIONS,
                            blink::mojom::PermissionsPolicyFeature::kNotFound) {
}

NotificationPermissionContext::~NotificationPermissionContext() {}

ContentSetting NotificationPermissionContext::GetPermissionStatusInternal(
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Extensions can declare the "notifications" permission in their manifest
  // that also grant permission to use the Web Notification API.
  ContentSetting extension_status =
      GetPermissionStatusForExtension(requesting_origin);
  if (extension_status != CONTENT_SETTING_ASK)
    return extension_status;
#endif

  ContentSetting setting =
      permissions::PermissionContextBase::GetPermissionStatusInternal(
          render_frame_host, requesting_origin, embedding_origin);

  content_settings::PageSpecificContentSettings::NotificationsAccessed(
      render_frame_host, /*blocked=*/setting != CONTENT_SETTING_ALLOW);

  if (requesting_origin != embedding_origin && setting == CONTENT_SETTING_ASK)
    return CONTENT_SETTING_BLOCK;

  return setting;
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
ContentSetting NotificationPermissionContext::GetPermissionStatusForExtension(
    const GURL& origin) const {
  constexpr ContentSetting kDefaultSetting = CONTENT_SETTING_ASK;
  if (!origin.SchemeIs(extensions::kExtensionScheme))
    return kDefaultSetting;

  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(
          Profile::FromBrowserContext(browser_context()))
          ->enabled_extensions()
          .GetByID(origin.host());

  if (!extension || !extension->permissions_data()->HasAPIPermission(
                        extensions::mojom::APIPermissionID::kNotifications)) {
    // The |extension| doesn't exist, or doesn't have the "notifications"
    // permission declared in their manifest
    return kDefaultSetting;
  }

  NotifierStateTracker* notifier_state_tracker =
      NotifierStateTrackerFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context()));
  DCHECK(notifier_state_tracker);

  message_center::NotifierId notifier_id(
      message_center::NotifierType::APPLICATION, extension->id());
  return notifier_state_tracker->IsNotifierEnabled(notifier_id)
             ? CONTENT_SETTING_ALLOW
             : CONTENT_SETTING_BLOCK;
}
#endif

void NotificationPermissionContext::DecidePermission(
    permissions::PermissionRequestData request_data,
    permissions::BrowserPermissionCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Permission requests for either Web Notifications and Push Notifications may
  // only happen on top-level frames and same-origin iframes. Usage will
  // continue to be allowed in all iframes: such frames could trivially work
  // around the restriction by posting a message to their Service Worker, where
  // showing a notification is allowed.
  if (request_data.requesting_origin != request_data.embedding_origin) {
    std::move(callback).Run(CONTENT_SETTING_BLOCK);
    return;
  }

  content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(
      request_data.id.global_render_frame_host_id());

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);

  // Notifications permission is always denied in incognito. To prevent sites
  // from using that to detect whether incognito mode is active, we deny after a
  // random time delay, to simulate a user clicking a bubble/infobar. See also
  // ContentSettingsRegistry::Init, which marks notifications as
  // INHERIT_IF_LESS_PERMISSIVE, and
  // PermissionMenuModel::PermissionMenuModel which prevents users from manually
  // allowing the permission.
  if (browser_context()->IsOffTheRecord()) {
    // Random number of seconds in the range [1.0, 2.0).
    double delay_seconds = 1.0 + 1.0 * base::RandDouble();
    VisibilityTimerTabHelper::CreateForWebContents(web_contents);
    VisibilityTimerTabHelper::FromWebContents(web_contents)
        ->PostTaskAfterVisibleDelay(
            FROM_HERE,
            base::BindOnce(&NotificationPermissionContext::NotifyPermissionSet,
                           weak_factory_ui_thread_.GetWeakPtr(),
                           request_data.id, request_data.requesting_origin,
                           request_data.embedding_origin, std::move(callback),
                           /*persist=*/true, CONTENT_SETTING_BLOCK,
                           /*is_one_time=*/false,
                           /*is_final_decision=*/true),
            base::Seconds(delay_seconds));
    return;
  }

#if BUILDFLAG(IS_ANDROID)
  bool contains_webapk = ShortcutHelper::DoesOriginContainAnyInstalledWebApk(
      request_data.requesting_origin);
  bool contains_twa =
      ShortcutHelper::DoesOriginContainAnyInstalledTrustedWebActivity(
          request_data.requesting_origin);
  bool contains_installed_webapp = contains_twa || contains_webapk;
  if (base::android::BuildInfo::GetInstance()->is_at_least_t() &&
      contains_installed_webapp) {
    // WebAPKs match URLs using a scope URL which may contain a path. An origin
    // has no path and would not fall within such a scope. So to find a matching
    // WebAPK we must pass a more complete URL e.g. GetLastCommittedURL.
    InstalledWebappBridge::DecidePermission(
        ContentSettingsType::NOTIFICATIONS, request_data.requesting_origin,
        web_contents->GetLastCommittedURL(),
        base::BindOnce(&NotificationPermissionContext::NotifyPermissionSet,
                       weak_factory_ui_thread_.GetWeakPtr(), request_data.id,
                       request_data.requesting_origin,
                       request_data.embedding_origin, std::move(callback),
                       /*persist=*/false));
    return;
  }
#endif  // BUILDFLAG(IS_ANDROID)

  permissions::PermissionContextBase::DecidePermission(std::move(request_data),
                                                       std::move(callback));
}

void NotificationPermissionContext::UpdateTabContext(
    const permissions::PermissionRequestID& id,
    const GURL& requesting_frame,
    bool allowed) {
  auto* content_settings =
      content_settings::PageSpecificContentSettings::GetForFrame(
          id.global_render_frame_host_id());
  if (!content_settings) {
    return;
  }

  if (allowed) {
    content_settings->OnContentAllowed(ContentSettingsType::NOTIFICATIONS);
  } else {
    content_settings->OnContentBlocked(ContentSettingsType::NOTIFICATIONS);
  }
}
