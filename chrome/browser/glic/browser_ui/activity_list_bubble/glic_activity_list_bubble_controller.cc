// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/activity_list_bubble/glic_activity_list_bubble_controller.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/ui/actor_ui_metrics.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager.h"
#include "chrome/browser/glic/browser_ui/activity_list_bubble/glic_activity_list_bubble_controller_delegate.h"
#include "chrome/browser/glic/browser_ui/activity_list_bubble/glic_activity_row_view_model.h"
#include "chrome/browser/glic/browser_ui/glic_split_button_controller.h"
#include "chrome/browser/glic/browser_ui/glic_split_button_delegate.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/service/glic_activity_manager.h"
#include "chrome/browser/glic/public/service/glic_instance_coordinator.h"
#include "chrome/browser/glic/public/service/legacy_glic_activity_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/base_window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/gfx/vector_icon_types.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/tab_android.h"
#endif

namespace glic {

namespace {

int GetPriorityForTaskState(actor::ActorTask::State task_state,
                            bool requires_processing,
                            glic::mojom::FeatureMode feature_mode) {
  // Tasks should be prioritized in the following order:
  // 1. Unprocessed tasks needing attention
  // 2. Processed tasks needing attention
  // 3. Remaining tasks that need processing
  // 4. All other tasks
  return LegacyGlicActivityManager::RequiresAttention(task_state)
             ? (requires_processing ? 1 : 2)
         : LegacyGlicActivityManager::RequiresTaskProcessing(task_state,
                                                             feature_mode)
             ? 3
             : 4;
}

bool ShouldShowConsentOverride(
    actor::ActorTask::State state,
    bool has_tab,
    std::optional<actor::ActorTask::InterruptReason> interrupt_reason) {
  if (!has_tab && !(state == actor::ActorTask::State::kActing ||
                    state == actor::ActorTask::State::kReflecting)) {
    return false;
  }
  return LegacyGlicActivityManager::RequiresAttention(state) &&
         interrupt_reason == actor::ActorTask::InterruptReason::
                                 kWaitingForExperimentalTriggeringConsent;
}

bool IsProcessedTabClosedRow(actor::ActorTask::State state,
                             bool has_tab,
                             bool requires_processing) {
  if (state == actor::ActorTask::State::kActing ||
      state == actor::ActorTask::State::kReflecting) {
    return false;
  }
  return !has_tab && !requires_processing;
}

const gfx::VectorIcon& GetRowIcon(actor::ActorTask::State state) {
  if (LegacyGlicActivityManager::RequiresAttention(state)) {
    return features::IsRoundedIconsEnabled() ? kHourglassIcon
                                             : kHourglassOldIcon;
  }
  if (state == actor::ActorTask::State::kFinished) {
    return features::IsRoundedIconsEnabled() ? kTaskSparkIcon
                                             : kTaskSparkOldIcon;
  }
  return kCursorSparkIcon;
}

ui::ColorId GetRowColor(bool is_enabled,
                        bool requires_processing,
                        bool needs_review) {
  if (!is_enabled) {
    return ui::kColorSysStateDisabled;
  }
  if (requires_processing && needs_review) {
    return ui::kColorSysPrimary;
  }
  return ui::kColorMenuIcon;
}

std::u16string GetRowSubtitle(
    actor::ActorTask::State state,
    bool has_tab,
    glic::mojom::FeatureMode feature_mode,
    std::optional<actor::ActorTask::InterruptReason> interrupt_reason) {
  if (ShouldShowConsentOverride(state, has_tab, interrupt_reason)) {
    return l10n_util::GetStringUTF16(
        IDS_GLIC_TASK_WAITING_FOR_CONSENT_SUBTITLE);
  }
  // If the task does not have a tab, show the "Tab closed" subtitle *unless*
  // the task is active (kActing or kReflecting). Active tasks may start with no
  // associated tab yet, so we avoid displaying "Tab closed" on them.
  if (!has_tab && !(state == actor::ActorTask::State::kActing ||
                    state == actor::ActorTask::State::kReflecting)) {
    return l10n_util::GetStringUTF16(
        IDS_ACTOR_TASK_LIST_BUBBLE_ROW_TAB_CLOSED_SUBTITLE);
  }
  if (LegacyGlicActivityManager::RequiresAttention(state)) {
    return l10n_util::GetStringUTF16(
        IDS_ACTOR_TASK_LIST_BUBBLE_ROW_CHECK_TASK_SUBTITLE);
  }
  if (state == actor::ActorTask::State::kFinished) {
    if (feature_mode == glic::mojom::FeatureMode::kExperimentalTriggering) {
      return l10n_util::GetStringUTF16(
          IDS_EXPERIMENTAL_TRIGGERING_TASK_LIST_BUBBLE_ROW_COMPLETED_TASK_SUBTITLE);
    }
    return l10n_util::GetStringUTF16(
        IDS_ACTOR_TASK_LIST_BUBBLE_ROW_COMPLETED_TASK_SUBTITLE);
  } else if (state == actor::ActorTask::State::kFailed) {
    return l10n_util::GetStringUTF16(
        IDS_ACTOR_TASK_LIST_BUBBLE_ROW_FAILED_TASK_SUBTITLE);
  } else if (state == actor::ActorTask::State::kPausedByUser) {
    return l10n_util::GetStringUTF16(
        IDS_ACTOR_TASK_LIST_BUBBLE_ROW_PAUSED_TASK_SUBTITLE);
  }
  return l10n_util::GetStringUTF16(
      IDS_ACTOR_TASK_LIST_BUBBLE_ROW_ACTING_TASK_SUBTITLE);
}

GlicActivityRowViewModel CreateActorTaskActivityRow(
    Profile* profile,
    const GlicActivityItem& item,
    base::RepeatingCallback<void(actor::TaskId)> on_task_clicked) {
  GlicActivityRowViewModel model;
  model.task_id = item.task_id;
  model.has_tab = item.has_tab;
  model.needs_review = (item.state == GlicActivityState::kNeedsAttention);
  model.has_been_seen = item.has_been_seen;
  model.is_enabled = item.is_enabled;
  model.icon = item.icon;
#if BUILDFLAG(IS_ANDROID)
  if (item.task_id.has_value()) {
    if (auto* actor_ui_state_manager =
            actor::ui::ActorUiStateManager::Get(profile)) {
      if (auto task_tab =
              actor_ui_state_manager->GetLastActedOnTab(*item.task_id);
          task_tab && *task_tab) {
        if (TabAndroid* tab_android = TabAndroid::FromTabInterface(*task_tab)) {
          model.tab_id = tab_android->GetAndroidId();
        }
      }
    }
  }
#endif
  model.icon_and_subtitle_color =
      GetRowColor(item.is_enabled, !item.has_been_seen, model.needs_review);
  model.title = item.title;
  model.subtitle = item.subtitle;
  if (item.task_id.has_value() && on_task_clicked) {
    model.on_clicked = base::BindRepeating(on_task_clicked, *item.task_id);
  }
  return model;
}

std::u16string GetConversationRowSubtitle(const GlicActivityItem& item) {
  if (!item.subtitle.empty()) {
    return item.subtitle;
  }
  switch (item.state) {
    case GlicActivityState::kInProgress:
      return l10n_util::GetStringUTF16(IDS_GLIC_ACTIVITY_WORKING);
    case GlicActivityState::kPaused:
      return l10n_util::GetStringUTF16(
          IDS_ACTOR_TASK_LIST_BUBBLE_ROW_PAUSED_TASK_SUBTITLE);
    case GlicActivityState::kCompleted:
      return item.has_been_seen
                 ? u""
                 : l10n_util::GetStringUTF16(IDS_GLIC_ACTIVITY_RESPONSE_READY);
    case GlicActivityState::kNeedsAttention:
      return l10n_util::GetStringUTF16(IDS_GLIC_ACTIVITY_NEEDS_INPUT);
    case GlicActivityState::kFailed:
      return l10n_util::GetStringUTF16(IDS_GLIC_ACTIVITY_ERROR);
  }
}

GlicActivityRowViewModel CreateConversationActivityRow(
    const GlicActivityItem& item,
    base::RepeatingCallback<void(const std::string&)> on_conversation_clicked) {
  const std::string& conv_id = item.key.id;
  GlicActivityRowViewModel model;
  model.conversation_id = conv_id;
  model.has_been_seen = item.has_been_seen;
  model.has_tab = true;
  model.is_enabled = true;
  model.icon = item.icon ? item.icon.get() : &vector_icons::kChatSparkIcon;
  model.icon_and_subtitle_color =
      !item.has_been_seen ? ui::kColorSysPrimary : ui::kColorMenuIcon;
  model.title = item.title.empty()
                    ? l10n_util::GetStringUTF16(IDS_GLIC_ACTIVITY_TITLE_GEMINI)
                    : item.title;
  model.subtitle = GetConversationRowSubtitle(item);
  if (on_conversation_clicked) {
    model.on_clicked = base::BindRepeating(on_conversation_clicked, conv_id);
  }
  return model;
}

}  // namespace

DEFINE_USER_DATA(GlicActivityListBubbleController);

// static
std::vector<GlicActivityRowViewModel>
GlicActivityListBubbleController::GetActorTaskRowViewModels(
    Profile* profile,
    const absl::flat_hash_map<actor::TaskId, bool>& task_list,
    base::RepeatingCallback<void(actor::TaskId)> on_task_clicked) {
  actor::ui::ActorUiStateManager* actor_ui_state_manager =
      actor::ui::ActorUiStateManager::Get(profile);
  if (!actor_ui_state_manager) {
    return {};
  }

  // Gather row data and priority in a single pass.
  std::vector<std::pair</*priority=*/int, GlicActivityRowViewModel>>
      prioritized_rows;
  prioritized_rows.reserve(task_list.size());

  for (auto [task_id, requires_processing] : task_list) {
    auto task_state = actor_ui_state_manager->GetActorTaskState(task_id);
    if (!task_state) {
      actor::ui::RecordTaskIconError(
          actor::ui::ActorUiTaskIconError::kBubbleTaskDoesntExist);
      continue;
    }

    auto feature_mode = actor_ui_state_manager->GetFeatureMode(task_id);
    const int priority = GetPriorityForTaskState(
        task_state.value(), requires_processing, feature_mode);

    auto task_title = actor_ui_state_manager->GetActorTaskTitle(task_id);
    auto task_tab = actor_ui_state_manager->GetLastActedOnTab(task_id);
    auto task_interrupt_reason =
        actor_ui_state_manager->GetActorTaskInterruptReason(task_id);

    CHECK(task_title.has_value() && task_tab.has_value());
    bool has_tab = task_tab.value() != nullptr;
    int tab_id = -1;

#if BUILDFLAG(IS_ANDROID)
    if (has_tab) {
      if (TabAndroid* tab_android =
              TabAndroid::FromTabInterface(task_tab.value())) {
        tab_id = tab_android->GetAndroidId();
      }
    }
#endif

    if (!has_tab && LegacyGlicActivityManager::IsActiveExperimentalTask(
                        task_state.value(), feature_mode)) {
      // Treat experimental triggering tasks as having a tab even if they don't
      // have one associated yet. This ensures they are clickable and can bring
      // the window/tab to the foreground.
      has_tab = true;
    }

    bool is_enabled = !IsProcessedTabClosedRow(task_state.value(), has_tab,
                                               requires_processing);
    bool needs_review =
        LegacyGlicActivityManager::RequiresAttention(task_state.value());

    GlicActivityRowViewModel model;
    model.task_id = task_id;
    model.has_tab = has_tab;
    model.tab_id = tab_id;
    model.needs_review = needs_review;
    model.has_been_seen = !requires_processing;
    model.icon = &GetRowIcon(task_state.value());
    model.icon_and_subtitle_color =
        GetRowColor(is_enabled, requires_processing, needs_review);
    model.is_enabled = is_enabled;

    model.title =
        ShouldShowConsentOverride(task_state.value(), has_tab,
                                  task_interrupt_reason)
            ? l10n_util::GetStringUTF16(IDS_GLIC_TASK_WAITING_FOR_CONSENT_TITLE)
            : base::UTF8ToUTF16(task_title.value());

    model.subtitle = GetRowSubtitle(task_state.value(), has_tab, feature_mode,
                                    task_interrupt_reason);

    if (on_task_clicked) {
      model.on_clicked = base::BindRepeating(on_task_clicked, task_id);
    }

    prioritized_rows.emplace_back(priority, std::move(model));
  }

  // Sort rows in order of priority, breaking ties by task_id for determinism.
  std::sort(prioritized_rows.begin(), prioritized_rows.end(),
            [](const auto& a, const auto& b) {
              if (a.first != b.first) {
                return a.first < b.first;
              }
              return a.second.task_id < b.second.task_id;
            });

  std::vector<GlicActivityRowViewModel> rows;
  rows.reserve(prioritized_rows.size());
  for (auto& [priority, row_data] : prioritized_rows) {
    rows.push_back(std::move(row_data));
  }

  return rows;
}

std::vector<GlicActivityRowViewModel>
GlicActivityListBubbleController::CreateRowViewModels() {
  std::vector<GlicActivityRowViewModel> rows = CreateRowViewModelsImpl();
  last_row_count_ = rows.size();
  return rows;
}

std::vector<GlicActivityRowViewModel>
GlicActivityListBubbleController::CreateRowViewModelsImpl() {
  Profile* profile = browser_->GetProfile();
  if (!base::FeatureList::IsEnabled(features::kGlicActivityHub)) {
    auto* manager = glic::LegacyGlicActivityManager::Get(profile);
    if (!manager) {
      return {};
    }

    return GetActorTaskRowViewModels(
        profile, manager->actor_task_list_bubble_rows(),
        base::BindRepeating(&GlicActivityListBubbleController::OnTaskRowClicked,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  auto* activity_manager = glic::GlicActivityManager::Get(profile);
  if (!activity_manager) {
    return {};
  }
  activity_manager->RefreshActivities();

  std::optional<std::string> active_tab_conv_id;
  bool is_active_tab_showing = false;
  if (tabs::TabInterface* active_tab =
          TabListInterface::From(browser_)
              ? TabListInterface::From(browser_)->GetActiveTab()
              : nullptr;
      active_tab) {
    if (auto* glic_service = GlicKeyedService::Get(profile)) {
      if (auto* instance =
              glic_service->instance_coordinator().GetInstanceForTab(
                  active_tab)) {
        active_tab_conv_id = instance->conversation_id();
        is_active_tab_showing = instance->IsShowing();
      }
    }
  }

  auto on_task_clicked =
      base::BindRepeating(&GlicActivityListBubbleController::OnTaskRowClicked,
                          weak_ptr_factory_.GetWeakPtr());
  auto on_conversation_clicked = base::BindRepeating(
      &GlicActivityListBubbleController::OnConversationRowClicked,
      weak_ptr_factory_.GetWeakPtr());

  std::vector<const GlicActivityItem*> sorted_items =
      activity_manager->GetSortedActivities(active_tab_conv_id,
                                            is_active_tab_showing);
  std::vector<GlicActivityRowViewModel> rows;
  rows.reserve(sorted_items.size());
  for (const auto* item_ptr : sorted_items) {
    if (item_ptr->type() == GlicActivityType::kActorTask) {
      rows.push_back(
          CreateActorTaskActivityRow(profile, *item_ptr, on_task_clicked));
    } else if (item_ptr->type() == GlicActivityType::kConversationResponse) {
      rows.push_back(
          CreateConversationActivityRow(*item_ptr, on_conversation_clicked));
    }
  }
  return rows;
}

GlicActivityListBubbleController::GlicActivityListBubbleController(
    BrowserWindowInterface* browser_window,
    GlicSplitButtonController& split_button_controller)
    : browser_(browser_window),
      split_button_controller_(split_button_controller),
      scoped_unowned_user_data_(browser_window->GetUnownedUserDataHost(),
                                *this) {
  CHECK(base::FeatureList::IsEnabled(features::kGlicActor) ||
        base::FeatureList::IsEnabled(features::kGlicActivityHub));
  if (base::FeatureList::IsEnabled(features::kGlicActivityHub)) {
    if (auto* activity_manager =
            glic::GlicActivityManager::Get(browser_->GetProfile())) {
      bubble_state_change_callback_subscription_.push_back(
          activity_manager->RegisterActivityAlertCallback(base::BindRepeating(
              &GlicActivityListBubbleController::OnActivityAlert,
              base::Unretained(this))));
    }
  } else if (auto* manager =
                 glic::LegacyGlicActivityManager::Get(browser_->GetProfile())) {
    bubble_state_change_callback_subscription_.push_back(
        manager->RegisterTaskListBubbleStateChange(base::BindRepeating(
            &GlicActivityListBubbleController::OnStateUpdate,
            base::Unretained(this))));
  }
}

GlicActivityListBubbleController::~GlicActivityListBubbleController() = default;

void GlicActivityListBubbleController::ShowBubble(bool is_start_notification) {
  const bool should_delay = !IsBubbleShowing() && is_start_notification &&
                            base::FeatureList::IsEnabled(
                                features::kGlicActorUiTaskListBubbleDelayShow);

  if (should_delay) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&GlicActivityListBubbleController::ShowBubbleImpl,
                       weak_ptr_factory_.GetWeakPtr(), is_start_notification),
        base::Milliseconds(features::kGlicActorUiTaskListBubbleDelayMs.Get()));
  } else {
    ShowBubbleImpl(is_start_notification);
  }
}

void GlicActivityListBubbleController::CloseBubble() {
  if (auto* delegate = split_button_controller_->GetActiveDelegate()) {
    delegate->CloseActivityListBubble();
  }
}

bool GlicActivityListBubbleController::IsBubbleShowing() const {
  auto* delegate = GetActiveDelegate();
  return delegate && delegate->IsActivityListBubbleShowing();
}

void GlicActivityListBubbleController::ShowBubbleImpl(
    bool is_start_notification) {
  auto* delegate = GetActiveDelegate();
  if (!delegate) {
    return;
  }

  const bool is_activity_hub =
      base::FeatureList::IsEnabled(features::kGlicActivityHub);
  auto* manager =
      !is_activity_hub
          ? glic::LegacyGlicActivityManager::Get(browser_->GetProfile())
          : nullptr;
  if (!is_activity_hub && !manager) {
    return;
  }

  // If the browser is in the background, only show the bubble if this is a
  // start notification for an experimentalTriggering task triggered while
  // the Glic panel is visible on this window. We avoid popping up the bubble
  // for subsequent background task status updates to avoid disturbing the user.
#if BUILDFLAG(IS_ANDROID)
  const bool is_active =
      browser_->GetWindow() && browser_->GetWindow()->IsActive();
#else
  const bool is_active = browser_->IsActive();
#endif
  if (!is_active) {
    // When kGlicExperimentalTriggeringOsNotification is enabled, we avoid
    // popping up the bubble on inactive windows entirely.
    if (base::FeatureList::IsEnabled(
            features::kGlicExperimentalTriggeringOsNotification)) {
      return;
    }
    auto* glic_service = glic::GlicKeyedService::Get(browser_->GetProfile());
    if (!is_start_notification || !glic_service ||
        !glic_service->IsPanelShowingForBrowser(*browser_)) {
      return;
    }
  }

  if (is_activity_hub) {
    if (CreateRowViewModels().empty()) {
      return;
    }
  } else {
    const auto& task_id_to_state = manager->actor_task_list_bubble_rows();
    // Do not show bubble if there are no rows to show.
    if (task_id_to_state.empty()) {
      return;
    }
  }

  // Close any existing bubble widget to avoid stacking multiple bubble windows.
  split_button_controller_->CallOnBoth(
      base::BindRepeating([](glic::GlicSplitButtonDelegate& delegate) {
        delegate.CloseActivityListBubble();
      }));
  delegate->ShowActivityListBubble();

  // All rows may be skipped, in which case the bubble will not be shown.
  if (delegate->IsActivityListBubbleShowing()) {
    on_bubble_shown_callback_list.Notify();
    actor::ui::RecordTaskListBubbleRows(last_row_count_);
  }
}

void GlicActivityListBubbleController::OnActivityAlert(
    ActivityInterruptionLevel level) {
  auto* delegate = GetActiveDelegate();
  if (!delegate) {
    return;
  }

  switch (level) {
    case ActivityInterruptionLevel::kDelayedTaskStart:
      ShowBubble(/*is_start_notification=*/true);
      break;
    case ActivityInterruptionLevel::kPassiveNudge:
    case ActivityInterruptionLevel::kSilent:
      if (delegate->IsActivityListBubbleShowing()) {
        ShowBubble(/*is_start_notification=*/false);
      }
      break;
  }
}

void GlicActivityListBubbleController::OnStateUpdate(
    bool is_start_notification) {
  auto* delegate = GetActiveDelegate();
  if (!delegate) {
    return;
  }

  if (delegate->IsActivityListBubbleShowing() || is_start_notification) {
    ShowBubble(is_start_notification);
  }
}

void GlicActivityListBubbleController::OnBubbleDestroyed() {
  on_bubble_destroyed_callback_list.Notify();
}

base::CallbackListSubscription
GlicActivityListBubbleController::RegisterBubbleShownCallback(
    base::RepeatingClosure callback) {
  return on_bubble_shown_callback_list.Add(std::move(callback));
}

base::CallbackListSubscription
GlicActivityListBubbleController::RegisterBubbleDestroyedCallback(
    base::RepeatingClosure callback) {
  return on_bubble_destroyed_callback_list.Add(std::move(callback));
}

void GlicActivityListBubbleController::OnTaskRowClicked(actor::TaskId task_id) {
  Profile* profile = browser_->GetProfile();
  actor::ui::ActorUiStateManager* manager =
      actor::ui::ActorUiStateManager::Get(profile);
  if (auto last_tab_opt = manager->GetLastActedOnTab(task_id);
      last_tab_opt && *last_tab_opt) {
    tabs::TabInterface* last_tab = *last_tab_opt;
    TabListInterface::From(last_tab->GetBrowserWindowInterface())
        ->ActivateTab(last_tab->GetHandle());
    // Activate the window that the tab is in as it may not be the current one.
    if (auto* window = last_tab->GetBrowserWindowInterface()->GetWindow()) {
      window->Activate();
    }
    if (auto* glic_service =
            glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile)) {
      glic_service->ShowUI(browser_,
                           glic::mojom::InvocationSource::kActorTaskIcon);
      if (auto* instance = glic_service->GetInstanceForTab(last_tab)) {
        instance->NotifyActorTaskListRowClicked(task_id.value());
      }
    }
  }
  // Regardless of tab navigation, process the row and close the bubble when
  // done.
  if (base::FeatureList::IsEnabled(features::kGlicActivityHub)) {
    if (auto* activity_manager = glic::GlicActivityManager::Get(profile)) {
      activity_manager->MarkActivitySeen(task_id);
    }
  } else if (auto* activity_manager =
                 glic::LegacyGlicActivityManager::Get(profile)) {
    activity_manager->ProcessRowInTaskListBubble(task_id);
  }
  CloseBubble();
  actor::ui::LogTaskListBubbleRowClicked();
}

void GlicActivityListBubbleController::OnConversationRowClicked(
    const std::string& conversation_id) {
  Profile* profile = browser_->GetProfile();
  if (auto* glic_service = GlicKeyedService::Get(profile)) {
    GlicActivityKey key{GlicActivityType::kConversationResponse,
                        conversation_id};
    glic_service->activity_manager().MarkActivitySeen(key);
    Target target;
    target.conversation = ConversationId(conversation_id);
    target.surface = LastActiveOrNew{};
    GlicInvokeOptions options(std::move(target),
                              mojom::InvocationSource::kTopChromeButton);
    glic_service->Invoke(std::move(options));
  }
  CloseBubble();
}

GlicActivityListBubbleControllerDelegate*
GlicActivityListBubbleController::GetActiveDelegate() const {
  return split_button_controller_->GetActiveDelegate();
}

// static
GlicActivityListBubbleController* GlicActivityListBubbleController::From(
    BrowserWindowInterface* browser) {
  return Get(browser->GetUnownedUserDataHost());
}

base::WeakPtr<GlicActivityListBubbleController>
GlicActivityListBubbleController::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace glic
