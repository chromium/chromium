// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_permission_context.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/circular_deque.h"
#include "base/location.h"
#include "base/rand_util.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/timer.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/permission_request_id.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/common/page_visibility_state.h"
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

// At most one of these is attached to each WebContents. It allows posting
// delayed tasks whose timer only counts down whilst the WebContents is visible
// (and whose timer is reset whenever the WebContents stops being visible).
class VisibilityTimerTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<VisibilityTimerTabHelper> {
 public:
  ~VisibilityTimerTabHelper() override {}

  // Runs |task| after the WebContents has been visible for a consecutive
  // duration of at least |visible_delay|.
  void PostTaskAfterVisibleDelay(const base::Location& from_here,
                                 base::OnceClosure task,
                                 base::TimeDelta visible_delay,
                                 const PermissionRequestID& id);

  // Deletes any earlier task(s) that match |id|.
  void CancelTask(const PermissionRequestID& id);

  // WebContentsObserver:
  void OnVisibilityChanged(content::Visibility visibility) override;
  void WebContentsDestroyed() override;

 private:
  friend class content::WebContentsUserData<VisibilityTimerTabHelper>;
  explicit VisibilityTimerTabHelper(content::WebContents* contents);

  void RunTask(base::OnceClosure task);

  bool is_visible_;

  struct Task {
    Task(const PermissionRequestID& id,
         std::unique_ptr<base::RetainingOneShotTimer> timer)
        : id(id), timer(std::move(timer)) {}

    // Move-only.
    Task(Task&&) noexcept = default;
    Task(const Task&) = delete;

    Task& operator=(Task&& other) {
      id = other.id;
      timer = std::move(other.timer);
      return *this;
    }

    PermissionRequestID id;
    std::unique_ptr<base::RetainingOneShotTimer> timer;
  };
  base::circular_deque<Task> task_queue_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(VisibilityTimerTabHelper);
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(VisibilityTimerTabHelper)

VisibilityTimerTabHelper::VisibilityTimerTabHelper(
    content::WebContents* contents)
    : content::WebContentsObserver(contents) {
  if (!contents->GetMainFrame()) {
    is_visible_ = false;
  } else {
    switch (contents->GetMainFrame()->GetVisibilityState()) {
      case content::PageVisibilityState::kHidden:
      case content::PageVisibilityState::kHiddenButPainting:
        is_visible_ = false;
        break;
      case content::PageVisibilityState::kVisible:
        is_visible_ = true;
        break;
    }
  }
}

void VisibilityTimerTabHelper::PostTaskAfterVisibleDelay(
    const base::Location& from_here,
    base::OnceClosure task,
    base::TimeDelta visible_delay,
    const PermissionRequestID& id) {
  if (web_contents()->IsBeingDestroyed())
    return;

  // Safe to use Unretained, as destroying |this| will destroy task_queue_,
  // hence cancelling all timers.
  // RetainingOneShotTimer is used which needs a RepeatingCallback, but we
  // only have it run this callback a single time, and destroy it after.
  auto timer = std::make_unique<base::RetainingOneShotTimer>(
      from_here, visible_delay,
      base::AdaptCallbackForRepeating(
          base::BindOnce(&VisibilityTimerTabHelper::RunTask,
                         base::Unretained(this), std::move(task))));
  DCHECK(!timer->IsRunning());

  task_queue_.emplace_back(id, std::move(timer));

  if (is_visible_ && task_queue_.size() == 1)
    task_queue_.front().timer->Reset();
}

void VisibilityTimerTabHelper::CancelTask(const PermissionRequestID& id) {
  bool deleting_front = task_queue_.front().id == id;

  base::EraseIf(task_queue_, [id](const Task& task) { return task.id == id; });

  if (!task_queue_.empty() && is_visible_ && deleting_front)
    task_queue_.front().timer->Reset();
}

void VisibilityTimerTabHelper::OnVisibilityChanged(
    content::Visibility visibility) {
  if (visibility == content::Visibility::VISIBLE) {
    if (!is_visible_ && !task_queue_.empty())
      task_queue_.front().timer->Reset();
    is_visible_ = true;
  } else {
    if (is_visible_ && !task_queue_.empty())
      task_queue_.front().timer->Stop();
    is_visible_ = false;
  }
}

void VisibilityTimerTabHelper::WebContentsDestroyed() {
  task_queue_.clear();
}

void VisibilityTimerTabHelper::RunTask(base::OnceClosure task) {
  DCHECK(is_visible_);
  std::move(task).Run();
  task_queue_.pop_front();
  if (!task_queue_.empty())
    task_queue_.front().timer->Reset();
}

}  // namespace

// static
void NotificationPermissionContext::UpdatePermission(Profile* profile,
                                                     const GURL& origin,
                                                     ContentSetting setting) {
  switch (setting) {
    case CONTENT_SETTING_ALLOW:
    case CONTENT_SETTING_BLOCK:
    case CONTENT_SETTING_DEFAULT:
      HostContentSettingsMapFactory::GetForProfile(profile)
          ->SetContentSettingDefaultScope(
              origin, GURL(), ContentSettingsType::NOTIFICATIONS,
              content_settings::ResourceIdentifier(), setting);
      break;

    default:
      NOTREACHED();
  }
}

NotificationPermissionContext::NotificationPermissionContext(Profile* profile)
    : PermissionContextBase(profile,
                            ContentSettingsType::NOTIFICATIONS,
                            blink::mojom::FeaturePolicyFeature::kNotFound) {}

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

  ContentSetting setting = PermissionContextBase::GetPermissionStatusInternal(
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
      extensions::ExtensionRegistry::Get(profile())
          ->enabled_extensions()
          .GetByID(origin.host());

  if (!extension || !extension->permissions_data()->HasAPIPermission(
                        extensions::APIPermission::kNotifications)) {
    // The |extension| doesn't exist, or doesn't have the "notifications"
    // permission declared in their manifest
    return kDefaultSetting;
  }

  NotifierStateTracker* notifier_state_tracker =
      NotifierStateTrackerFactory::GetForProfile(profile());
  DCHECK(notifier_state_tracker);

  message_center::NotifierId notifier_id(
      message_center::NotifierType::APPLICATION, extension->id());
  return notifier_state_tracker->IsNotifierEnabled(notifier_id)
             ? CONTENT_SETTING_ALLOW
             : CONTENT_SETTING_BLOCK;
}
#endif

void NotificationPermissionContext::ResetPermission(
    const GURL& requesting_origin,
    const GURL& embedder_origin) {
  UpdatePermission(profile(), requesting_origin, CONTENT_SETTING_DEFAULT);
}

void NotificationPermissionContext::DecidePermission(
    content::WebContents* web_contents,
    const PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    bool user_gesture,
    BrowserPermissionCallback callback) {
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
  if (profile()->IsOffTheRecord()) {
    // Random number of seconds in the range [1.0, 2.0).
    double delay_seconds = 1.0 + 1.0 * base::RandDouble();
    VisibilityTimerTabHelper::CreateForWebContents(web_contents);
    VisibilityTimerTabHelper::FromWebContents(web_contents)
        ->PostTaskAfterVisibleDelay(
            FROM_HERE,
            base::BindOnce(&NotificationPermissionContext::NotifyPermissionSet,
                           weak_factory_ui_thread_.GetWeakPtr(), id,
                           requesting_origin, embedding_origin,
                           std::move(callback), true /* persist */,
                           CONTENT_SETTING_BLOCK),
            base::TimeDelta::FromSecondsD(delay_seconds), id);
    return;
  }

  PermissionContextBase::DecidePermission(web_contents, id, requesting_origin,
                                          embedding_origin, user_gesture,
                                          std::move(callback));
}

// Unlike other permission types, granting a notification for a given origin
// will not take into account the |embedder_origin|, it will only be based
// on the requesting iframe origin.
// TODO(mukai) Consider why notifications behave differently than
// other permissions. https://crbug.com/416894
void NotificationPermissionContext::UpdateContentSetting(
    const GURL& requesting_origin,
    const GURL& embedder_origin,
    ContentSetting content_setting) {
  DCHECK(content_setting == CONTENT_SETTING_ALLOW ||
         content_setting == CONTENT_SETTING_BLOCK);
  UpdatePermission(profile(), requesting_origin, content_setting);
}

bool NotificationPermissionContext::IsRestrictedToSecureOrigins() const {
  return true;
}
