// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_permission_context.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/rand_util.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/visibility_timer_tab_helper.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_request_id.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents_user_data.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "url/gurl.h"

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

namespace {

class NotificationTabHelper
    : public content::WebContentsUserData<NotificationTabHelper> {
 public:
  ~NotificationTabHelper() override = default;

  void set_should_block_new_notification_requests(bool value) {
    should_block_new_notification_requests_ = value;
  }

  bool should_block_new_notification_requests() {
    return should_block_new_notification_requests_;
  }

 private:
  explicit NotificationTabHelper(content::WebContents* contents) {}

  friend class content::WebContentsUserData<NotificationTabHelper>;

  bool should_block_new_notification_requests_ = false;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(NotificationTabHelper)

}  // namespace

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
      NOTREACHED();
  }
}

NotificationPermissionContext::NotificationPermissionContext(
    content::BrowserContext* browser_context)
    : PermissionContextBase(browser_context,
                            ContentSettingsType::NOTIFICATIONS,
                            blink::mojom::PermissionsPolicyFeature::kNotFound) {
}

NotificationPermissionContext::~NotificationPermissionContext() {}

// static
void NotificationPermissionContext::SetBlockNewNotificationRequests(
    content::WebContents* web_contents,
    bool value) {
  NotificationTabHelper::CreateForWebContents(web_contents);
  NotificationTabHelper::FromWebContents(web_contents)
      ->set_should_block_new_notification_requests(value);
}

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
    content::WebContents* web_contents,
    const permissions::PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    bool user_gesture,
    permissions::BrowserPermissionCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Permission requests for either Web Notifications and Push Notifications may
  // only happen on top-level frames and same-origin iframes. Usage will
  // continue to be allowed in all iframes: such frames could trivially work
  // around the restriction by posting a message to their Service Worker, where
  // showing a notification is allowed.
  if (requesting_origin != embedding_origin) {
    std::move(callback).Run(CONTENT_SETTING_BLOCK);
    return;
  }

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
                           weak_factory_ui_thread_.GetWeakPtr(), id,
                           requesting_origin, embedding_origin,
                           std::move(callback), /*persist=*/true,
                           CONTENT_SETTING_BLOCK, /*is_one_time=*/false),
            base::TimeDelta::FromSecondsD(delay_seconds));
    return;
  }

  auto* tab_helper = NotificationTabHelper::FromWebContents(web_contents);
  if (tab_helper && tab_helper->should_block_new_notification_requests()) {
    std::move(callback).Run(CONTENT_SETTING_BLOCK);
    return;
  }

  permissions::PermissionContextBase::DecidePermission(
      web_contents, id, requesting_origin, embedding_origin, user_gesture,
      std::move(callback));
}

bool NotificationPermissionContext::IsRestrictedToSecureOrigins() const {
  return true;
}
