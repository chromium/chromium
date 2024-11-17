// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/scheduled_task_handler/reboot_notifications_scheduler.h"

#include <algorithm>
#include <optional>

#include "ash/constants/ash_pref_names.h"
#include "base/check_is_test.h"
#include "base/containers/small_map.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "chrome/browser/ash/app_restore/full_restore_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_prefs/user_prefs.h"

namespace policy {
namespace {
constexpr base::TimeDelta kNotificationDelay = base::Hours(1);
constexpr base::TimeDelta kDialogDelay = base::Minutes(5);

const char* ToString(RebootNotificationsScheduler::Requester requester) {
#define CASE(name)                                    \
  case RebootNotificationsScheduler::Requester::name: \
    return #name;

  switch (requester) {
    CASE(kScheduledRebootPolicy);
    CASE(kRebootCommand);
  }
#undef CASE
}
}  // namespace

// Handles the notification request queue ordered by reboot time with current
// request being the earliest request and all other being pending. Does not
// guarantee requests order with the same reboot time.
class RebootNotificationsScheduler::RequestQueue {
 public:
  struct Request {
    // Scheduled reboot time.
    base::Time reboot_time;
    // Callback to run on "Reboot now" button click.
    RebootButtonCallback reboot_button_callback;
  };

  struct RequsterAndRebootTime {
    Requester requester;
    base::Time reboot_time;

    bool operator==(const RequsterAndRebootTime& other) const {
      return requester == other.requester && reboot_time == other.reboot_time;
    }
  };

  RequestQueue() = default;
  ~RequestQueue() = default;

  RequestQueue(const RequestQueue&) = delete;
  RequestQueue& operator=(const RequestQueue&) = delete;

  // Returns the requester with the earliest reboot time.
  std::optional<RequsterAndRebootTime> current_request() const {
    if (requests_.empty()) {
      return std::nullopt;
    }
    const auto it = std::min_element(requests_.begin(), requests_.end(),
                                     [](const auto& first, const auto& second) {
                                       return first.second.reboot_time <
                                              second.second.reboot_time;
                                     });
    DCHECK(it != requests_.end());

    return RequsterAndRebootTime{.requester = it->first,
                                 .reboot_time = it->second.reboot_time};
  }

  std::vector<Requester> GetRequestersForTesting() const {
    CHECK_IS_TEST();

    std::vector<Requester> requesters;
    for (const auto& [requester, request] : requests_) {
      requesters.push_back(requester);
    }

    return requesters;
  }

  // The very last method to call after which the queue is invalid.
  [[nodiscard]] RebootButtonCallback TakeCallback() {
    const std::optional<RequsterAndRebootTime> current = current_request();

    if (!current.has_value()) {
      return base::DoNothing();
    }

    auto reboot_callback =
        std::move(requests_[current->requester].reboot_button_callback);

    if (!reboot_callback) {
      return base::DoNothing();
    }

    return reboot_callback;
  }

  // Returns true if the new request takes place. Returns false if the current
  // request does not change.
  [[nodiscard]] bool Reschedule(Requester requester, Request request) {
    const auto original_top_request = current_request();

    requests_[requester] = std::move(request);
    DCHECK(current_request());

    return original_top_request != current_request();
  }

  // Returns true if the current request is being reset. Returns false if the
  // current request does not change.
  [[nodiscard]] bool Reset(Requester requester) {
    const std::optional<RequsterAndRebootTime> current = current_request();

    requests_.erase(requester);

    const bool was_current_request = current && current->requester == requester;
    return was_current_request;
  }

 private:
  // Holds requests per `Requester`.
  base::small_map<std::map<Requester, Request>,
                  static_cast<size_t>(Requester::kMaxValue) + 1>
      requests_;
};

RebootNotificationsScheduler* RebootNotificationsScheduler::instance = nullptr;

RebootNotificationsScheduler::RebootNotificationsScheduler()
    : RebootNotificationsScheduler(base::DefaultClock::GetInstance(),
                                   base::DefaultTickClock::GetInstance()) {}

RebootNotificationsScheduler::RebootNotificationsScheduler(
    const base::Clock* clock,
    const base::TickClock* tick_clock)
    : requester_queue_(std::make_unique<RequestQueue>()),
      notification_timer_(clock, tick_clock),
      dialog_timer_(clock, tick_clock),
      clock_(clock) {
  DCHECK(!RebootNotificationsScheduler::Get());
  RebootNotificationsScheduler::SetInstance(this);
  if (session_manager::SessionManager::Get())
    observation_.Observe(session_manager::SessionManager::Get());
}

RebootNotificationsScheduler::~RebootNotificationsScheduler() {
  DCHECK_EQ(instance, this);
  observation_.Reset();
  RebootNotificationsScheduler::SetInstance(nullptr);
}

// static
RebootNotificationsScheduler* RebootNotificationsScheduler::Get() {
  return RebootNotificationsScheduler::instance;
}

// static
void RebootNotificationsScheduler::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(ash::prefs::kShowPostRebootNotification, false);
}

// static
bool RebootNotificationsScheduler::ShouldShowPostRebootNotification(
    Profile* profile) {
  DCHECK(profile);
  PrefService* prefs = user_prefs::UserPrefs::Get(profile);
  return IsPostRebootPrefSet(prefs);
}

void RebootNotificationsScheduler::SchedulePendingRebootNotifications(
    RebootButtonCallback reboot_button_callback,
    const base::Time& reboot_time,
    Requester requester) {
  if (!requester_queue_->Reschedule(
          requester, RequestQueue::Request{.reboot_time = reboot_time,
                                           .reboot_button_callback = std::move(
                                               reboot_button_callback)})) {
    LOG(WARNING) << "Notification request is pending: " << ToString(requester)
                 << " at " << reboot_time;
    return;
  }

  LOG(WARNING) << "Notification is rescheduled: "
               << ToString(requester_queue_->current_request()->requester)
               << " at " << requester_queue_->current_request()->reboot_time;

  SchedulePendingRebootNotificationsForCurrentRequester();
}

void RebootNotificationsScheduler::
    SchedulePendingRebootNotificationsForCurrentRequester() {
  DCHECK(requester_queue_->current_request()) << "A request must be scheduled.";

  ResetNotificationState();

  base::TimeDelta delay =
      GetRebootDelay(requester_queue_->current_request()->reboot_time);

  if (delay > kNotificationDelay) {
    base::Time timer_run_time =
        requester_queue_->current_request()->reboot_time - kNotificationDelay;
    notification_timer_.Start(
        FROM_HERE, timer_run_time,
        base::BindOnce(
            &RebootNotificationsScheduler::MaybeShowPendingRebootNotification,
            weak_ptr_factory_.GetWeakPtr()));
  } else {
    MaybeShowPendingRebootNotification();
  }

  if (delay > kDialogDelay) {
    base::Time timer_run_time =
        requester_queue_->current_request()->reboot_time - kDialogDelay;
    dialog_timer_.Start(
        FROM_HERE, timer_run_time,
        base::BindOnce(
            &RebootNotificationsScheduler::MaybeShowPendingRebootDialog,
            weak_ptr_factory_.GetWeakPtr()));
    return;
  }
  MaybeShowPendingRebootDialog();
}

void RebootNotificationsScheduler::SchedulePostRebootNotification() {
  PrefService* prefs = GetPrefsForActiveProfile();
  if (prefs) {
    prefs->SetBoolean(ash::prefs::kShowPostRebootNotification, true);
  }
}

void RebootNotificationsScheduler::OnUserSessionStarted(bool is_primary_user) {
  // Return if we need to wait for the initialization of full restore service.
  if (ShouldWaitFullRestoreInit())
    return;

  MaybeShowPostRebootNotification(true /*show_simple_notification*/);
}

void RebootNotificationsScheduler::MaybeShowPostRebootNotification(
    bool show_simple_notification) {
  PrefService* prefs = GetPrefsForActiveProfile();
  // Return if the pref is not set for the profile.
  if (!IsPostRebootPrefSet(prefs))
    return;

  if (show_simple_notification) {
    notification_controller_.MaybeShowPostRebootNotification();
  }
  prefs->SetBoolean(ash::prefs::kShowPostRebootNotification, false);
  // No need to observe any more, since we showed the post reboot notification,
  // either as a simple one or integrated with full restore.
  observation_.Reset();
}

std::optional<RebootNotificationsScheduler::Requester>
RebootNotificationsScheduler::GetCurrentRequesterForTesting() const {
  CHECK_IS_TEST();
  return requester_queue_->current_request()
             ? requester_queue_->current_request()->requester
             : std::optional<Requester>(std::nullopt);
}

std::vector<RebootNotificationsScheduler::Requester>
RebootNotificationsScheduler::GetRequestersForTesting() const {
  CHECK_IS_TEST();
  return requester_queue_->GetRequestersForTesting();  // IN-TEST
}

void RebootNotificationsScheduler::CancelRebootNotifications(
    Requester requester) {
  if (!requester_queue_->Reset(requester)) {
    // The current notification request did not change. Nothing to reset or
    // reschedule.
    return;
  }

  // The current notification request changed: either reschedule for a new one
  // taken from pending or hide notifications if there's no more pending.

  if (requester_queue_->current_request()) {
    SchedulePendingRebootNotificationsForCurrentRequester();
  } else {
    ResetNotificationState();
  }
}

void RebootNotificationsScheduler::ResetNotificationState() {
  if (notification_timer_.IsRunning())
    notification_timer_.Stop();
  if (dialog_timer_.IsRunning())
    dialog_timer_.Stop();
  CloseNotifications();
}

void RebootNotificationsScheduler::MaybeShowPendingRebootNotification() {
  DCHECK(requester_queue_->current_request());
  notification_controller_.MaybeShowPendingRebootNotification(
      requester_queue_->current_request()->reboot_time,
      base::BindRepeating(&RebootNotificationsScheduler::OnRebootButtonClicked,
                          base::Unretained(this)));
}

void RebootNotificationsScheduler::MaybeShowPendingRebootDialog() {
  DCHECK(requester_queue_->current_request());
  notification_controller_.MaybeShowPendingRebootDialog(
      requester_queue_->current_request()->reboot_time,
      base::BindOnce(&RebootNotificationsScheduler::OnRebootButtonClicked,
                     base::Unretained(this)));
}

PrefService* RebootNotificationsScheduler::GetPrefsForActiveProfile() const {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (!profile)
    return nullptr;
  return user_prefs::UserPrefs::Get(profile);
}

void RebootNotificationsScheduler::OnRebootButtonClicked() {
  std::move(requester_queue_->TakeCallback()).Run();
}

void RebootNotificationsScheduler::SetInstance(
    RebootNotificationsScheduler* reboot_notifications_scheduler) {
  RebootNotificationsScheduler::instance = reboot_notifications_scheduler;
}

base::TimeDelta RebootNotificationsScheduler::GetRebootDelay(
    const base::Time& reboot_time) const {
  return reboot_time - clock_->Now();
}

void RebootNotificationsScheduler::CloseNotifications() {
  notification_controller_.CloseRebootNotification();
  notification_controller_.CloseRebootDialog();
}

bool RebootNotificationsScheduler::ShouldWaitFullRestoreInit() const {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  return ash::full_restore::FullRestoreServiceFactory::
      IsFullRestoreAvailableForProfile(profile);
}

bool RebootNotificationsScheduler::IsPostRebootPrefSet(PrefService* prefs) {
  if (!prefs)
    return false;
  return prefs->GetBoolean(ash::prefs::kShowPostRebootNotification);
}

}  // namespace policy
