// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_restore/full_restore_service.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/glanceables/post_login_glanceables_metrics_recorder.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/shell.h"
#include "ash/utility/forest_util.h"
#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "ash/webui/settings/public/constants/setting.mojom-shared.h"
#include "ash/wm/desks/templates/saved_desk_controller.h"
#include "ash/wm/window_restore/informed_restore_contents_data.h"
#include "ash/wm/window_restore/pine_controller.h"
#include "ash/wm/window_restore/window_restore_util.h"
#include "base/barrier_callback.h"
#include "base/command_line.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/trace_event/trace_event.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ash/app_restore/app_restore_arc_task_handler.h"
#include "chrome/browser/ash/app_restore/full_restore_app_launch_handler.h"
#include "chrome/browser/ash/app_restore/full_restore_data_handler.h"
#include "chrome/browser/ash/app_restore/full_restore_prefs.h"
#include "chrome/browser/ash/app_restore/full_restore_service_factory.h"
#include "chrome/browser/ash/app_restore/new_user_restore_pref_handler.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/reboot_notifications_scheduler.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/full_restore/full_restore_util.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/app_session_service_factory.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/account_id/account_id.h"
#include "components/app_constants/constants.h"
#include "components/app_restore/app_restore_info.h"
#include "components/app_restore/app_restore_utils.h"
#include "components/app_restore/features.h"
#include "components/app_restore/full_restore_save_handler.h"
#include "components/app_restore/full_restore_utils.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/message_center/public/cpp/notification.h"

// Enable VLOG level 1.
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace ash::full_restore {

namespace {

// This flag forces full session restore on startup regardless of potential
// non-clean shutdown. It could be used in tests to ignore crashes on shutdown.
constexpr char kForceFullRestoreAndSessionRestoreAfterCrash[] =
    "force-full-restore-and-session-restore-after-crash";

constexpr char kRestoreSettingHistogramName[] = "Apps.RestoreSetting";
constexpr char kRestoreInitSettingHistogramName[] = "Apps.RestoreInitSetting";
constexpr char kFullRestoreWindowCountHistogramName[] =
    "Apps.FullRestoreWindowCount2";

// If the reboot occurred due to DeviceScheduledRebootPolicy, change the title
// to notify the user that the device was rebooted by the administrator.
int GetRestoreNotificationTitleId(Profile* profile) {
  if (policy::RebootNotificationsScheduler::ShouldShowPostRebootNotification(
          profile)) {
    return IDS_POLICY_DEVICE_POST_REBOOT_TITLE;
  }
  return IDS_RESTORE_NOTIFICATION_TITLE;
}

// Returns true if `profile` is the primary user profile.
bool IsPrimaryUser(Profile* profile) {
  return ProfileHelper::Get()->GetUserByProfile(profile) ==
         user_manager::UserManager::Get()->GetPrimaryUser();
}

// Will (maybe) initiate an auto launch of an admin template.
void MaybeInitiateAdminTemplateAutoLaunch() {
  // The controller is available if the admin template feature is enabled.
  if (auto* saved_desk_controller = ash::SavedDeskController::Get()) {
    saved_desk_controller->InitiateAdminTemplateAutoLaunch(base::DoNothing());
  }
}

}  // namespace

bool g_restore_for_testing = true;

const char kRestoreForCrashNotificationId[] = "restore_for_crash_notification";
const char kRestoreNotificationId[] = "restore_notification";

const char kRestoreNotificationHistogramName[] = "Apps.RestoreNotification";
const char kRestoreForCrashNotificationHistogramName[] =
    "Apps.RestoreForCrashNotification";

bool MaybeCreateFullRestoreServiceForLacros() {
  // Full restore for Lacros depends on BrowserAppInstanceRegistry to save and
  // restore Lacros windows, so check the web apps crosapi flag to make sure
  // BrowserAppInstanceRegistry is created.
  if (!::full_restore::features::IsFullRestoreForLacrosEnabled() ||
      !web_app::IsWebAppsCrosapiEnabled()) {
    return false;
  }

  const user_manager::User* user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  DCHECK(user);
  Profile* profile = ProfileHelper::Get()->GetProfileByUser(user);
  DCHECK(profile);

  // Lacros can be launched at the very early stage during the system startup
  // phase. So create FullRestoreService to construct LacrosWindowHandler to
  // observe BrowserAppInstanceRegistry for Lacros windows before the first
  // Lacros window is created, to avoid missing any Lacros windows.
  return FullRestoreService::GetForProfile(profile);
}

class DelegateImpl : public FullRestoreService::Delegate {
 public:
  DelegateImpl() = default;
  DelegateImpl(const DelegateImpl&) = delete;
  DelegateImpl& operator=(const DelegateImpl&) = delete;
  ~DelegateImpl() override = default;

  void MaybeStartInformedRestoreOverviewSession(
      std::unique_ptr<InformedRestoreContentsData> contents_data) override {
    // A unit test that does not override this default delegate may not have ash
    // shell.
    if (Shell::HasInstance()) {
      CHECK(Shell::Get()->pine_controller());
      Shell::Get()->pine_controller()->MaybeStartPineOverviewSession(
          std::move(contents_data));
    }
  }

  void MaybeEndInformedRestoreOverviewSession() override {
    // A unit test that does not override this default delegate may not have ash
    // shell.
    if (Shell::HasInstance()) {
      CHECK(Shell::Get()->pine_controller());
      Shell::Get()->pine_controller()->MaybeEndPineOverviewSession();
    }
  }
};

// static
FullRestoreService* FullRestoreService::GetForProfile(Profile* profile) {
  TRACE_EVENT0("ui", "FullRestoreService::GetForProfile");
  return static_cast<FullRestoreService*>(
      FullRestoreServiceFactory::GetInstance()->GetForProfile(profile));
}

// static
void FullRestoreService::MaybeCloseNotification(Profile* profile) {
  auto* full_restore_service = FullRestoreService::GetForProfile(profile);
  if (full_restore_service)
    full_restore_service->MaybeCloseNotification();
}

FullRestoreService::FullRestoreService(Profile* profile)
    : profile_(profile),
      app_launch_handler_(std::make_unique<FullRestoreAppLaunchHandler>(
          profile_,
          /*should_init_service=*/true)),
      restore_data_handler_(std::make_unique<FullRestoreDataHandler>(profile_)),
      delegate_(std::make_unique<DelegateImpl>()) {
  on_app_terminating_subscription_ =
      browser_shutdown::AddAppTerminatingCallback(base::BindOnce(
          &FullRestoreService::OnAppTerminating, base::Unretained(this)));

  auto* full_restore_save_handler =
      ::full_restore::FullRestoreSaveHandler::GetInstance();
  full_restore_save_handler->InsertIgnoreApplicationId(
      web_app::kOsFeedbackAppId);

  PrefService* prefs = profile_->GetPrefs();
  DCHECK(prefs);

  pref_change_registrar_.Init(prefs);
  pref_change_registrar_.Add(
      prefs::kRestoreAppsAndPagesPrefName,
      base::BindRepeating(&FullRestoreService::OnPreferenceChanged,
                          weak_ptr_factory_.GetWeakPtr()));

  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(profile_);
  if (user) {
    ::app_restore::AppRestoreInfo::GetInstance()->SetRestorePref(
        user->GetAccountId(), CanPerformRestore(prefs));
  }

  // Set profile path before init the restore process to create
  // FullRestoreSaveHandler to observe restore windows.
  if (IsPrimaryUser(profile_)) {
    full_restore_save_handler->SetPrimaryProfilePath(profile_->GetPath());

    // In Multi-Profile mode, only set for the primary user. For other users,
    // active profile path is set when switch users.
    ::full_restore::SetActiveProfilePath(profile_->GetPath());

    can_be_inited_ = CanBeInited();
  }

  if (!HasRestorePref(prefs) && HasSessionStartupPref(prefs)) {
    // If there is no full restore pref, but there is a session restore setting,
    // set the first run flag to maintain the previous behavior for the first
    // time running the full restore feature when migrate to the full restore
    // release. Restore browsers and web apps by the browser session restore.
    first_run_full_restore_ = true;
    SetDefaultRestorePrefIfNecessary(prefs);
    full_restore_save_handler->AllowSave();
    VLOG(1) << "No restore pref! First time to run full restore."
            << profile_->GetPath();
  }
}

FullRestoreService::~FullRestoreService() = default;

void FullRestoreService::Init(bool& show_notification) {
  // If it is the first time to migrate to the full restore release, we don't
  // have other restore data, so we don't need to consider restoration.
  if (first_run_full_restore_)
    return;

  // If the user of `profile_` is not the primary user, and hasn't been the
  // active user yet, we don't need to consider restoration to prevent the
  // restored windows are written to the active user's profile path.
  if (!can_be_inited_)
    return;

  // If the restore data has not been loaded, wait for it. For test cases,
  // `app_launch_handler_` might be reset as null because test cases might be
  // finished before Init is called, so check `app_launch_handler_` to prevent
  // crash for test cases.
  if (!app_launch_handler_ || !app_launch_handler_->IsRestoreDataLoaded())
    return;

  if (is_shut_down_)
    return;

  PrefService* prefs = profile_->GetPrefs();
  DCHECK(prefs);

  // If the system crashed before reboot, show the restore notification.
  if (ExitTypeService::GetLastSessionExitType(profile_) == ExitType::kCrashed) {
    if (!HasRestorePref(prefs))
      SetDefaultRestorePrefIfNecessary(prefs);

    MaybeShowRestoreNotification(kRestoreForCrashNotificationId,
                                 show_notification);
    return;
  }

  // If either OS pref setting nor Chrome pref setting exist, that means we
  // don't have restore data, so we don't need to consider restoration, and call
  // NewUserRestorePrefHandler to set OS pref setting.
  if (!HasRestorePref(prefs) && !HasSessionStartupPref(prefs)) {
    new_user_pref_handler_ =
        std::make_unique<NewUserRestorePrefHandler>(profile_);
    ::full_restore::FullRestoreSaveHandler::GetInstance()->AllowSave();
    MaybeInitiateAdminTemplateAutoLaunch();
    return;
  }

  RestoreOption restore_pref = static_cast<RestoreOption>(
      prefs->GetInteger(prefs::kRestoreAppsAndPagesPrefName));
  base::UmaHistogramEnumeration(kRestoreInitSettingHistogramName, restore_pref);

  // Record the window count from the full restore file, unless the option is do
  // not restore.
  if (restore_pref != RestoreOption::kDoNotRestore) {
    ::app_restore::RestoreData* restore_data =
        app_launch_handler_->restore_data();
    if (!restore_data) {
      base::UmaHistogramCounts100(kFullRestoreWindowCountHistogramName, 0);
    } else {
      auto [window_count, tab_count, total_count] =
          ::app_restore::GetWindowAndTabCount(*restore_data);
      base::UmaHistogramCounts100(kFullRestoreWindowCountHistogramName,
                                  window_count);
    }
  }

  switch (restore_pref) {
    case RestoreOption::kAlways:
      Restore();
      break;
    case RestoreOption::kAskEveryTime:
      MaybeShowRestoreNotification(kRestoreNotificationId, show_notification);
      MaybeInitiateAdminTemplateAutoLaunch();
      break;
    case RestoreOption::kDoNotRestore:
      if (IsForestFeatureEnabled()) {
        MaybeShowInformedRestoreOnboarding();
      }
      ::full_restore::FullRestoreSaveHandler::GetInstance()->AllowSave();
      MaybeInitiateAdminTemplateAutoLaunch();
      return;
  }
}

void FullRestoreService::OnTransitionedToNewActiveUser(Profile* profile) {
  const bool already_initialized = can_be_inited_;
  if (profile_ != profile || already_initialized)
    return;

  can_be_inited_ = true;
  bool show_notification = false;
  Init(show_notification);
}

void FullRestoreService::LaunchBrowserWhenReady() {
  TRACE_EVENT0("ui", "FullRestoreService::LaunchBrowserWhenReady");
  if (!g_restore_for_testing || !app_launch_handler_)
    return;

  app_launch_handler_->LaunchBrowserWhenReady(first_run_full_restore_);
}

void FullRestoreService::MaybeCloseNotification(bool allow_save) {
  close_notification_ = true;
  VLOG(1) << "The full restore notification is closed for "
          << profile_->GetPath();

  // The crash notification creates a crash lock for the browser session
  // restore. So if the notification has been closed and the system is no longer
  // crash, clear `crashed_lock_`. Otherwise, the crash flag might not be
  // cleared, and the crash notification might be shown again after the normal
  // shutdown process.
  crashed_lock_.reset();

  if (notification_ && !is_shut_down_) {
    NotificationDisplayService::GetForProfile(profile_)->Close(
        NotificationHandler::Type::TRANSIENT, notification_->id());
    accelerator_controller_observer_.Reset();
  }

  if (allow_save) {
    // If the user launches an app or clicks the cancel button, start the save
    // timer.
    ::full_restore::FullRestoreSaveHandler::GetInstance()->AllowSave();
  }
}

void FullRestoreService::Restore() {
  if (app_launch_handler_)
    app_launch_handler_->SetShouldRestore();
}

void FullRestoreService::Close(bool by_user) {
  if (!skip_notification_histogram_) {
    RecordRestoreAction(
        notification_->id(),
        by_user ? RestoreAction::kCloseByUser : RestoreAction::kCloseNotByUser);
  }
  notification_ = nullptr;

  if (by_user) {
    // If the user closes the notification, start the save timer. If it is not
    // closed by the user, the restore button might be clicked, then we need to
    // wait for the restore finish to start the save timer.
    ::full_restore::FullRestoreSaveHandler::GetInstance()->AllowSave();
  }
}

void FullRestoreService::Click(const std::optional<int>& button_index,
                               const std::optional<std::u16string>& reply) {
  DCHECK(notification_);
  skip_notification_histogram_ = true;

  if (!button_index.has_value() ||
      button_index.value() ==
          static_cast<int>(RestoreNotificationButtonIndex::kRestore)) {
    VLOG(1) << "The restore notification is clicked for "
            << profile_->GetPath();

    // Restore if the user clicks the notification body.
    RecordRestoreAction(notification_->id(), RestoreAction::kRestore);
    Restore();

    // If the user selects restore, don't start the save timer. Wait for the
    // restore finish.
    MaybeCloseNotification(/*allow_save=*/false);
    return;
  }

  if (notification_->id() == kRestoreNotificationId) {
    // Show the 'On Startup' OS setting page if the user clicks the settings
    // button of the restore notification.
    ash::features::IsOsSettingsRevampWayfindingEnabled()
        ? chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
              profile_,
              chromeos::settings::mojom::kSystemPreferencesSectionPath,
              chromeos::settings::mojom::Setting::kRestoreAppsAndPages)
        : chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
              profile_, chromeos::settings::mojom::kAppsSectionPath);

    return;
  }

  VLOG(1) << "The crash restore notification is canceled for "
          << profile_->GetPath();

  // Close the crash notification if the user clicks the cancel button of the
  // crash notification.
  RecordRestoreAction(notification_->id(), RestoreAction::kCancel);
  MaybeCloseNotification();
}

void FullRestoreService::OnActionPerformed(AcceleratorAction action) {
  switch (action) {
    case AcceleratorAction::kNewIncognitoWindow:
    case AcceleratorAction::kNewTab:
    case AcceleratorAction::kNewWindow:
    case AcceleratorAction::kOpenCrosh:
    case AcceleratorAction::kOpenDiagnostics:
    case AcceleratorAction::kRestoreTab:
      MaybeCloseNotification();
      return;
    default:
      return;
  }
}

void FullRestoreService::OnAcceleratorControllerWillBeDestroyed(
    AcceleratorController* controller) {
  accelerator_controller_observer_.Reset();
}

void FullRestoreService::SetAppLaunchHandlerForTesting(
    std::unique_ptr<FullRestoreAppLaunchHandler> app_launch_handler) {
  app_launch_handler_ = std::move(app_launch_handler);
}

void FullRestoreService::Shutdown() {
  is_shut_down_ = true;
}

bool FullRestoreService::CanBeInited() const {
  auto* user_manager = user_manager::UserManager::Get();
  DCHECK(user_manager);
  DCHECK(user_manager->GetActiveUser());

  // For non-primary user, wait for `OnTransitionedToNewActiveUser`.
  auto* user = ProfileHelper::Get()->GetUserByProfile(profile_);
  if (user != user_manager->GetPrimaryUser()) {
    VLOG(1) << "Can't init full restore service for non_primary user."
            << profile_->GetPath();
    return false;
  }

  // Check the command line to decide whether this is the restart case.
  // `kLoginManager` means starting Chrome with login/oobe screen, not the
  // restart process. For the restart process, `kLoginUser` should be in the
  // command line.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  DCHECK(command_line);
  if (command_line->HasSwitch(switches::kLoginManager) ||
      !command_line->HasSwitch(switches::kLoginUser)) {
    return true;
  }

  // When the system restarts, and the active user in the previous session is
  // not the primary user, don't init, but wait for the transition to the last
  // active user.
  const auto& last_session_active_account_id =
      user_manager->GetLastSessionActiveAccountId();
  if (last_session_active_account_id.is_valid() &&
      AccountId::FromUserEmail(user->GetAccountId().GetUserEmail()) !=
          last_session_active_account_id) {
    VLOG(1) << "Can't init full restore service for non-active primary user."
            << profile_->GetPath();
    return false;
  }

  return true;
}

void FullRestoreService::MaybeShowRestoreNotification(const std::string& id,
                                                      bool& show_notification) {
  if (!app_launch_handler_) {
    return;
  }

  // Do not show the notification if we have no restore data.
  if (!IsForestFeatureEnabled() && !app_launch_handler_->HasRestoreData()) {
    return;
  }

  // Do not show the notification if it is the first run or the notification is
  // being closed.
  if (::first_run::IsChromeFirstRun() || close_notification_) {
    return;
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kForceFullRestoreAndSessionRestoreAfterCrash)) {
    LOG(WARNING) << "Full session restore was forced by a debug flag.";
    Restore();
    return;
  }

  const bool last_session_crashed = id == kRestoreForCrashNotificationId;
  if (!app_launch_handler_->HasRestoreData()) {
    CHECK(IsForestFeatureEnabled());
    MaybeShowInformedRestoreOnboarding();
    return;
  }
  CHECK(app_launch_handler_->HasRestoreData());

  // If the system is restored from crash, create the crash lock for the browser
  // session restore to help set the browser saving flag.
  ExitTypeService* exit_type_service =
      ExitTypeService::GetInstanceForProfile(profile_);
  if (last_session_crashed && exit_type_service) {
    crashed_lock_ = exit_type_service->CreateCrashedLock();
  }

  if (Shell::HasInstance()) {
    Shell::Get()
        ->post_login_glanceables_metrics_reporter()
        ->RecordPostLoginFullRestoreShown();
  }

  if (IsForestFeatureEnabled()) {
    CHECK(delegate_);

    if (crosapi::browser_util::IsLacrosEnabled()) {
      crosapi::CrosapiManager::Get()
          ->crosapi_ash()
          ->full_restore_ash()
          ->GetSessionInformation(base::BindOnce(
              &FullRestoreService::OnGotAllSessionsLacros,
              weak_ptr_factory_.GetWeakPtr(), last_session_crashed));
    } else {
      // Retrieves session service data from browser and app browsers, which
      // will be used to display favicons and tab titles.
      SessionServiceBase* service =
          SessionServiceFactory::GetForProfileForSessionRestore(profile_);
      SessionServiceBase* app_service =
          AppSessionServiceFactory::GetForProfileForSessionRestore(profile_);
      if (service && app_service) {
        auto barrier = base::BarrierCallback<SessionWindows>(
            /*num_callbacks=*/2u, /*done_callback=*/base::BindOnce(
                &FullRestoreService::OnGotAllSessionsAsh,
                weak_ptr_factory_.GetWeakPtr(), last_session_crashed));

        service->GetLastSession(
            base::BindOnce(&FullRestoreService::OnGotSessionAsh,
                           weak_ptr_factory_.GetWeakPtr(), barrier));
        app_service->GetLastSession(
            base::BindOnce(&FullRestoreService::OnGotSessionAsh,
                           weak_ptr_factory_.GetWeakPtr(), barrier));
      } else {
        OnGotAllSessionsAsh(last_session_crashed, /*all_session_windows=*/{});
      }
    }

    // Set to true as we might want to show the post reboot notification.
    show_notification = true;
    return;
  }

  // For forest, we will handle closing the dialog on the ash side.
  if (auto* accelerator_controller = AcceleratorController::Get()) {
    CHECK(!accelerator_controller_observer_.IsObserving());
    accelerator_controller_observer_.Observe(accelerator_controller);
  }

  message_center::RichNotificationData notification_data;

  message_center::ButtonInfo restore_button(
      l10n_util::GetStringUTF16(IDS_RESTORE_NOTIFICATION_RESTORE_BUTTON));
  notification_data.buttons.push_back(restore_button);

  int button_id;
  if (id == kRestoreForCrashNotificationId)
    button_id = IDS_RESTORE_NOTIFICATION_CANCEL_BUTTON;
  else
    button_id = IDS_RESTORE_NOTIFICATION_SETTINGS_BUTTON;
  message_center::ButtonInfo cancel_button(
      l10n_util::GetStringUTF16(button_id));
  notification_data.buttons.push_back(cancel_button);

  std::u16string title;
  if (id == kRestoreForCrashNotificationId) {
    title = l10n_util::GetStringFUTF16(IDS_RESTORE_CRASH_NOTIFICATION_TITLE,
                                       ui::GetChromeOSDeviceName());
    VLOG(1) << "Show the restore notification for crash for "
            << profile_->GetPath();
  } else {
    title = l10n_util::GetStringUTF16(GetRestoreNotificationTitleId(profile_));
    VLOG(1) << "Show the restore notification for the normal startup for "
            << profile_->GetPath();
  }

  int message_id;
  if (id == kRestoreForCrashNotificationId)
    message_id = IDS_RESTORE_CRASH_NOTIFICATION_MESSAGE;
  else
    message_id = IDS_RESTORE_NOTIFICATION_MESSAGE;

  notification_ = CreateSystemNotificationPtr(
      message_center::NOTIFICATION_TYPE_SIMPLE, id, title,
      l10n_util::GetStringUTF16(message_id),
      l10n_util::GetStringUTF16(IDS_RESTORE_NOTIFICATION_DISPLAY_SOURCE),
      GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 id, NotificationCatalogName::kFullRestore),
      notification_data,
      base::MakeRefCounted<message_center::ThunkNotificationDelegate>(
          weak_ptr_factory_.GetWeakPtr()),
      kFullRestoreNotificationIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);
  notification_->set_priority(message_center::SYSTEM_PRIORITY);

  auto* notification_display_service =
      NotificationDisplayService::GetForProfile(profile_);
  DCHECK(notification_display_service);
  notification_display_service->Display(NotificationHandler::Type::TRANSIENT,
                                        *notification_,
                                        /*metadata=*/nullptr);
  show_notification = true;
}

void FullRestoreService::RecordRestoreAction(const std::string& notification_id,
                                             RestoreAction restore_action) {
  base::UmaHistogramEnumeration(notification_id == kRestoreNotificationId
                                    ? kRestoreNotificationHistogramName
                                    : kRestoreForCrashNotificationHistogramName,
                                restore_action);
}

void FullRestoreService::OnPreferenceChanged(const std::string& pref_name) {
  DCHECK_EQ(pref_name, prefs::kRestoreAppsAndPagesPrefName);

  RestoreOption restore_option = static_cast<RestoreOption>(
      profile_->GetPrefs()->GetInteger(prefs::kRestoreAppsAndPagesPrefName));
  base::UmaHistogramEnumeration(kRestoreSettingHistogramName, restore_option);

  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(profile_);
  if (user) {
    ::app_restore::AppRestoreInfo::GetInstance()->SetRestorePref(
        user->GetAccountId(), CanPerformRestore(profile_->GetPrefs()));
  }
}

void FullRestoreService::OnAppTerminating() {
  if (auto* arc_task_handler =
          app_restore::AppRestoreArcTaskHandler::GetForProfile(profile_)) {
    arc_task_handler->Shutdown();
  }
  app_launch_handler_.reset();
  ::full_restore::FullRestoreSaveHandler::GetInstance()->SetShutDown();
}

void FullRestoreService::OnDialogRestore() {
  VLOG(1) << "The restore button is clicked for " << profile_->GetPath();

  Restore();
  delegate_->MaybeEndInformedRestoreOverviewSession();
}

void FullRestoreService::OnDialogCancel() {
  ::full_restore::FullRestoreSaveHandler::GetInstance()->AllowSave();
  delegate_->MaybeEndInformedRestoreOverviewSession();
}

void FullRestoreService::OnGotSessionAsh(
    base::OnceCallback<void(SessionWindows)> callback,
    SessionWindows session_windows,
    SessionID active_window_id,
    bool read_error) {
  std::move(callback).Run(std::move(session_windows));
}

void FullRestoreService::OnGotAllSessionsAsh(
    bool last_session_crashed,
    const std::vector<SessionWindows>& all_session_windows) {
  // Place all the session windows in map so we don't have to do so many O(n)
  // lookups below. Note that this has the additional overhead of creating the
  // full_restore.mojom struct. This is so we can share more code with Lacros,
  // which is the final goal.
  SessionWindowsMap session_windows_map;
  for (const SessionWindows& session_windows : all_session_windows) {
    for (const std::unique_ptr<sessions::SessionWindow>& session_window :
         session_windows) {
      session_windows_map.emplace(
          session_window->window_id.id(),
          ::full_restore::ToSessionWindowPtr(*session_window,
                                             /*lacros_profile_id=*/0));
    }
  }

  OnSessionInformationReceived(app_launch_handler_->restore_data(),
                               session_windows_map, last_session_crashed);
}

void FullRestoreService::OnGotAllSessionsLacros(
    bool last_session_crashed,
    std::vector<crosapi::mojom::SessionWindowPtr> all_session_windows) {
  // Place all the session windows in map so we don't have to do so many O(n)
  // lookups below.
  SessionWindowsMap session_windows_map;
  for (const crosapi::mojom::SessionWindowPtr& session_window :
       all_session_windows) {
    session_windows_map.emplace(session_window->window_id,
                                session_window->Clone());
  }

  OnSessionInformationReceived(app_launch_handler_->restore_data(),
                               session_windows_map, last_session_crashed);
}

void FullRestoreService::OnSessionInformationReceived(
    ::app_restore::RestoreData* restore_data,
    const SessionWindowsMap& session_windows_map,
    bool last_session_crashed) {
  auto contents_data = std::make_unique<InformedRestoreContentsData>();
  contents_data->last_session_crashed = last_session_crashed;

  contents_data->restore_callback = base::BindOnce(
      &FullRestoreService::OnDialogRestore, weak_ptr_factory_.GetWeakPtr());
  contents_data->cancel_callback = base::BindOnce(
      &FullRestoreService::OnDialogCancel, weak_ptr_factory_.GetWeakPtr());

  // Contains per-window app data to be sorted and and added to
  // `contents_data`.
  struct WindowAppData {
    int window_id;
    std::string app_id;
    raw_ptr<::app_restore::AppRestoreData> app_restore_data;
  };

  // Retrieve app id's from `restore_data`. There can be multiple entries with
  // the same app id, these denote different windows.
  std::vector<WindowAppData> complete_window_list;
  for (const auto& [app_id, launch_list] :
       restore_data->app_id_to_launch_list()) {
    for (const std::pair<const int,
                         std::unique_ptr<::app_restore::AppRestoreData>>&
             id_data_pair : launch_list) {
      complete_window_list.emplace_back(id_data_pair.first, app_id,
                                        id_data_pair.second.get());
    }
  }

  // Sort the windows based on their activation index (more recent windows have
  // a lower index). Windows without an activation index can be placed at the
  // end.
  base::ranges::sort(complete_window_list, [](const WindowAppData& element_a,
                                              const WindowAppData& element_b) {
    return element_a.app_restore_data->window_info.activation_index.value_or(
               INT_MAX) <
           element_b.app_restore_data->window_info.activation_index.value_or(
               INT_MAX);
  });

  for (auto info : complete_window_list) {
    const int window_id = info.window_id;
    const std::string app_id = info.app_id;
    const std::string stored_title =
        base::UTF16ToUTF8(info.app_restore_data->window_info.app_title.value_or(
            std::u16string()));

    // For non browsers, the app id and title is sufficient for the UI we want
    // to display.
    if (app_id != app_constants::kChromeAppId &&
        app_id != app_constants::kLacrosAppId) {
      contents_data->apps_infos.emplace_back(app_id, stored_title);
      continue;
    }

    // Find the `crosapi::mojom::SessionWindow` associated with `window_id` if
    // it exists.
    auto it = session_windows_map.find(window_id);

    crosapi::mojom::SessionWindow* session_window =
        it == session_windows_map.end() ? nullptr : it->second.get();

    // Default to using the app id if we cannot find the associated window for
    // whatever reason.
    if (!session_window) {
      contents_data->apps_infos.emplace_back(app_id, stored_title);
      continue;
    }

    // App browsers app ID is the same as regular chrome browsers. To get the
    // correct icon and title from the app service, we need to find the app
    // name and remove the "_crx_", then use that result.
    const std::string app_name = session_window->app_name;
    if (!app_name.empty()) {
      const std::string new_app_id =
          ::app_restore::GetAppIdFromAppName(app_name);
      contents_data->apps_infos.emplace_back(
          new_app_id.empty() ? app_id : new_app_id, stored_title);
      continue;
    }

    contents_data->apps_infos.emplace_back(
        app_id, session_window->active_tab_title, session_window->urls,
        session_window->tab_count, session_window->profile_id);
  }

  delegate_->MaybeStartInformedRestoreOverviewSession(std::move(contents_data));
}

void FullRestoreService::MaybeShowInformedRestoreOnboarding() {
  if (Shell::HasInstance()) {
    RestoreOption restore_pref = static_cast<RestoreOption>(
        profile_->GetPrefs()->GetInteger(prefs::kRestoreAppsAndPagesPrefName));
    CHECK(Shell::Get()->pine_controller());
    Shell::Get()->pine_controller()->MaybeShowInformedRestoreOnboarding(
        /*restore_on=*/restore_pref == RestoreOption::kAskEveryTime);
  }
}

ScopedRestoreForTesting::ScopedRestoreForTesting() {
  g_restore_for_testing = false;
}

ScopedRestoreForTesting::~ScopedRestoreForTesting() {
  g_restore_for_testing = true;
}

}  // namespace ash::full_restore
