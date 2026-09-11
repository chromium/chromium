// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/glic_actor_task_notification_handler.h"

#include <string_view>

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager_interface.h"
#include "chrome/browser/glic/browser_ui/glic_actor_task_icon_manager.h"
#include "chrome/browser/glic/browser_ui/glic_actor_task_icon_manager_factory.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/base_window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"

namespace {
constexpr char kActorTaskStartNotificationPrefix[] = "actor_task_start_";
constexpr char kNotifierId[] = "glic_actor_task";

std::string GetNotificationId(actor::TaskId task_id) {
  return base::StrCat({kActorTaskStartNotificationPrefix,
                       base::NumberToString(task_id.value())});
}
}  // namespace

GlicActorTaskNotificationHandler::GlicActorTaskNotificationHandler() = default;
GlicActorTaskNotificationHandler::~GlicActorTaskNotificationHandler() = default;

// static
void GlicActorTaskNotificationHandler::MaybeShow(Profile* profile,
                                                 actor::TaskId task_id) {
  if (!profile || !base::FeatureList::IsEnabled(
                      features::kGlicExperimentalTriggeringOsNotification)) {
    return;
  }

  // Only show the OS notification if there are no active browser windows for
  // the profile.
  auto* collection = ProfileBrowserCollection::GetForProfile(profile);
  BrowserWindowInterface* last_active =
      collection ? collection->GetLastActiveBrowser() : nullptr;
  if (last_active && last_active->IsActive()) {
    return;
  }

  auto* notification_service =
      NotificationDisplayServiceFactory::GetForProfile(profile);
  if (!notification_service) {
    return;
  }

  auto* actor_service = actor::ActorKeyedService::Get(profile);
  if (!actor_service) {
    return;
  }
  auto* state_manager = actor_service->GetActorUiStateManager();
  if (!state_manager) {
    return;
  }

  std::u16string title = l10n_util::GetStringUTF16(
      IDS_ACTOR_EXPERIMENTAL_TRIGGERING_NOTIFICATION_TITLE);
  std::optional<std::string> task_title =
      state_manager->GetActorTaskTitle(task_id);
  std::u16string body =
      task_title && !task_title->empty()
          ? l10n_util::GetStringFUTF16(
                IDS_ACTOR_EXPERIMENTAL_TRIGGERING_NOTIFICATION_BODY,
                base::UTF8ToUTF16(*task_title))
          : l10n_util::GetStringUTF16(
                IDS_ACTOR_EXPERIMENTAL_TRIGGERING_NOTIFICATION_BODY_FALLBACK);

  std::string notification_id = GetNotificationId(task_id);
  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id, title, body,
      ui::ImageModel(),
      /*display_source=*/std::u16string(),
      /*origin_url=*/GURL(),
      message_center::NotifierId(message_center::NotifierType::APPLICATION,
                                 kNotifierId),
      message_center::RichNotificationData(),
      /*delegate=*/nullptr);

  notification_service->Display(NotificationHandler::Type::GLIC_ACTOR_TASK,
                                notification, /*metadata=*/nullptr);
}

// static
void GlicActorTaskNotificationHandler::Close(Profile* profile,
                                             actor::TaskId task_id) {
  if (!profile || !base::FeatureList::IsEnabled(
                      features::kGlicExperimentalTriggeringOsNotification)) {
    return;
  }

  auto* notification_service =
      NotificationDisplayServiceFactory::GetForProfile(profile);
  if (!notification_service) {
    return;
  }

  notification_service->Close(NotificationHandler::Type::GLIC_ACTOR_TASK,
                              GetNotificationId(task_id));
}

void GlicActorTaskNotificationHandler::OnClick(
    Profile* profile,
    const GURL& origin,
    const std::string& notification_id,
    const std::optional<int>& action_index,
    const std::optional<std::u16string>& reply,
    base::OnceClosure completed_closure) {
  base::ScopedClosureRunner runner(std::move(completed_closure));
  if (!profile ||
      !notification_id.starts_with(kActorTaskStartNotificationPrefix)) {
    return;
  }

  uint32_t task_id_val = 0;
  if (!base::StringToUint(
          std::string_view(notification_id)
              .substr(std::size(kActorTaskStartNotificationPrefix) - 1),
          &task_id_val)) {
    return;
  }
  actor::TaskId task_id(task_id_val);

  if (auto* notification_service =
          NotificationDisplayServiceFactory::GetForProfile(profile)) {
    notification_service->Close(NotificationHandler::Type::GLIC_ACTOR_TASK,
                                notification_id);
  }

  if (auto* icon_manager =
          glic::GlicActorTaskIconManagerFactory::GetForProfile(profile)) {
    icon_manager->ProcessRowInTaskListBubble(task_id);
  }

  auto* actor_service = actor::ActorKeyedService::Get(profile);
  if (!actor_service) {
    return;
  }
  auto* state_manager = actor_service->GetActorUiStateManager();
  if (!state_manager) {
    return;
  }

  std::optional<tabs::TabInterface*> last_tab_opt =
      state_manager->GetLastActedOnTab(task_id);
  if (!last_tab_opt || !*last_tab_opt) {
    return;
  }
  tabs::TabInterface* task_tab = *last_tab_opt;

  BrowserWindowInterface* browser = task_tab->GetBrowserWindowInterface();
  if (!browser) {
    return;
  }

  TabListInterface::From(browser)->ActivateTab(task_tab->GetHandle());
  if (auto* window = browser->GetWindow()) {
    window->Activate();
  }
}
