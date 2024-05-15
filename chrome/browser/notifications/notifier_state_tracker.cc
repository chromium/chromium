// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notifier_state_tracker.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_result.h"
#include "extensions/buildflags/buildflags.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/common/extensions/api/notifications.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/extension_util.h"
#endif

using message_center::NotifierId;

// static
void NotifierStateTracker::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kMessageCenterDisabledExtensionIds);
}

NotifierStateTracker::NotifierStateTracker(Profile* profile)
    : profile_(profile) {
  OnStringListPrefChanged(
      prefs::kMessageCenterDisabledExtensionIds, &disabled_extension_ids_);

  disabled_extension_id_pref_.Init(
      prefs::kMessageCenterDisabledExtensionIds, profile_->GetPrefs(),
      base::BindRepeating(
          &NotifierStateTracker::OnStringListPrefChanged,
          base::Unretained(this),
          base::Unretained(prefs::kMessageCenterDisabledExtensionIds),
          base::Unretained(&disabled_extension_ids_)));

#if BUILDFLAG(ENABLE_EXTENSIONS)
  extension_registry_observation_.Observe(
      extensions::ExtensionRegistry::Get(profile_));
#endif
}

NotifierStateTracker::~NotifierStateTracker() {
}

bool NotifierStateTracker::IsNotifierEnabled(
    const NotifierId& notifier_id) const {
  switch (notifier_id.type) {
    case message_center::NotifierType::APPLICATION:
      return disabled_extension_ids_.find(notifier_id.id) ==
          disabled_extension_ids_.end();
    case message_center::NotifierType::WEB_PAGE:
      return profile_->GetPermissionController()
                 ->GetPermissionResultForOriginWithoutContext(
                     blink::PermissionType::NOTIFICATIONS,
                     url::Origin::Create(notifier_id.url))
                 .status == blink::mojom::PermissionStatus::GRANTED;
    case message_center::NotifierType::SYSTEM_COMPONENT:
      // We do not disable system component notifications.
      return true;
    case message_center::NotifierType::ARC_APPLICATION:
#if BUILDFLAG(IS_CHROMEOS_ASH)
      // TODO(hriono): Ask Android if the application's notifications are
      // enabled.
      return true;
#else
      break;
#endif
    case message_center::NotifierType::CROSTINI_APPLICATION:
#if BUILDFLAG(IS_CHROMEOS_ASH)
      // Disabling Crostini notifications is not supported yet.
      return true;
#else
      NOTREACHED_IN_MIGRATION();
      break;
#endif
    case message_center::NotifierType::PHONE_HUB:
#if BUILDFLAG(IS_CHROMEOS_ASH)
      // PhoneHub notifications are controlled in their own settings.
      return true;
#else
      break;
#endif
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

void NotifierStateTracker::SetNotifierEnabled(
    const NotifierId& notifier_id,
    bool enabled) {
  DCHECK_NE(message_center::NotifierType::WEB_PAGE, notifier_id.type);

  bool add_new_item = false;
  const char* pref_name = nullptr;
  base::Value id;
  switch (notifier_id.type) {
    case message_center::NotifierType::APPLICATION:
#if BUILDFLAG(ENABLE_EXTENSIONS)
      pref_name = prefs::kMessageCenterDisabledExtensionIds;
      add_new_item = !enabled;
      id = base::Value(notifier_id.id);
      FirePermissionLevelChangedEvent(notifier_id, enabled);
#else
      NOTREACHED_IN_MIGRATION();
#endif
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  DCHECK(pref_name != nullptr);

  ScopedListPrefUpdate update(profile_->GetPrefs(), pref_name);
  base::Value::List& update_list = update.Get();
  if (add_new_item) {
    if (!base::Contains(update_list, id))
      update_list.Append(std::move(id));
  } else {
    update_list.EraseValue(id);
  }
}

void NotifierStateTracker::OnStringListPrefChanged(
    const char* pref_name, std::set<std::string>* ids_field) {
  ids_field->clear();
  const base::Value::List& pref_list = profile_->GetPrefs()->GetList(pref_name);
  for (size_t i = 0; i < pref_list.size(); ++i) {
    const std::string* element = pref_list[i].GetIfString();
    if (element && !element->empty())
      ids_field->insert(*element);
    else
      LOG(WARNING) << i << "-th element is not a string for " << pref_name;
  }
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
void NotifierStateTracker::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UninstallReason reason) {
  NotifierId notifier_id(message_center::NotifierType::APPLICATION,
                         extension->id());
  if (IsNotifierEnabled(notifier_id))
    return;

  SetNotifierEnabled(notifier_id, true);
}

void NotifierStateTracker::FirePermissionLevelChangedEvent(
    const NotifierId& notifier_id, bool enabled) {
  DCHECK_EQ(message_center::NotifierType::APPLICATION, notifier_id.type);
  extensions::EventRouter* event_router =
      extensions::EventRouter::Get(profile_);
  if (!event_router) {
    // The |event_router| can be a nullptr in tests.
    return;
  }

  extensions::api::notifications::PermissionLevel permission =
      enabled ? extensions::api::notifications::PermissionLevel::kGranted
              : extensions::api::notifications::PermissionLevel::kDenied;
  base::Value::List args;
  args.Append(extensions::api::notifications::ToString(permission));
  auto event = std::make_unique<extensions::Event>(
      extensions::events::NOTIFICATIONS_ON_PERMISSION_LEVEL_CHANGED,
      extensions::api::notifications::OnPermissionLevelChanged::kEventName,
      std::move(args));

  event_router->DispatchEventToExtension(notifier_id.id, std::move(event));
}
#endif
