// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/hats/hats_notification_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/escape.h"
#include "base/task/thread_pool.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ash/hats/hats_config.h"
#include "chrome/browser/ash/hats/hats_dialog.h"
#include "chrome/browser/ash/hats/hats_finch_helper.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/version/version_loader.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/locale_util.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/strings/grit/ui_strings.h"

namespace ash {

namespace {

const char kNotificationOriginUrl[] = "chrome://hats";

const char kNotifierHats[] = "ash.hats";

// Minimum amount of time before the notification is displayed again after a
// user has interacted with it.
constexpr base::TimeDelta kHatsThreshold = base::Days(60);

// The state specific UMA enumerations
const int kSurveyTriggeredEnumeration = 1;

// TODO(jackshira): Migrate this to a manager class.
// Delimiters used to join the separate device info elements into a single
// string to be used as site context.
const char kDeviceInfoStopKeyword[] = "&";
const char kDeviceInfoKeyValueDelimiter[] = "=";
const char kDefaultProfileLocale[] = "en-US";

// TODO(jackshira): Migrate this to a manager class.
enum class DeviceInfoKey : unsigned int {
  BROWSER = 0,
  PLATFORM,
  FIRMWARE,
  LOCALE,
};

// TODO(jackshira): Migrate this to a manager class.
// Maps the given DeviceInfoKey |key| enum to the corresponding string value
// that can be used as a key when creating a URL parameter.
const std::string KeyEnumToString(DeviceInfoKey key) {
  switch (key) {
    case DeviceInfoKey::BROWSER:
      return "browser";
    case DeviceInfoKey::PLATFORM:
      return "platform";
    case DeviceInfoKey::FIRMWARE:
      return "firmware";
    case DeviceInfoKey::LOCALE:
      return "locale";
    default:
      NOTREACHED();
      return std::string();
  }
}

// Returns true if the given |profile| interacted with HaTS by either
// dismissing the notification or taking the survey within a given
// |threshold_time|.
bool DidShowSurveyToProfileRecently(Profile* profile,
                                    base::TimeDelta threshold_time) {
  int64_t serialized_timestamp =
      profile->GetPrefs()->GetInt64(prefs::kHatsLastInteractionTimestamp);

  base::Time previous_interaction_timestamp =
      base::Time::FromInternalValue(serialized_timestamp);
  return previous_interaction_timestamp + threshold_time > base::Time::Now();
}

// Returns true if at least |new_device_threshold| time has passed since
// OOBE. This is an indirect measure of whether the owner has used the device
// for at least |new_device_threshold| time.
bool IsNewDevice(base::TimeDelta new_device_threshold) {
  return StartupUtils::GetTimeSinceOobeFlagFileCreation() <=
         new_device_threshold;
}

// Returns true if the |kForceHappinessTrackingSystem| flag is enabled for the
// current survey.
bool IsTestingEnabled(const HatsConfig& hats_config) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  if (command_line->HasSwitch(switches::kForceHappinessTrackingSystem)) {
    auto switch_value = command_line->GetSwitchValueASCII(
        switches::kForceHappinessTrackingSystem);
    return switch_value.empty() || hats_config.feature.name == switch_value;
  }

  return false;
}

}  // namespace

// static
const char HatsNotificationController::kNotificationId[] = "hats_notification";

HatsNotificationController::HatsNotificationController(
    Profile* profile,
    const HatsConfig& hats_config,
    const base::flat_map<std::string, std::string>& product_specific_data)
    : profile_(profile),
      hats_config_(hats_config),
      product_specific_data_(product_specific_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::string histogram_name = HatsFinchHelper::GetHistogramName(hats_config_);
  if (!histogram_name.empty()) {
    base::UmaHistogramSparse(histogram_name, kSurveyTriggeredEnumeration);
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&IsNewDevice, hats_config.new_device_threshold),
      base::BindOnce(&HatsNotificationController::Initialize,
                     weak_pointer_factory_.GetWeakPtr()));
}

HatsNotificationController::HatsNotificationController(
    Profile* profile,
    const HatsConfig& hats_config)
    : HatsNotificationController(profile,
                                 hats_config,
                                 base::flat_map<std::string, std::string>()) {}

HatsNotificationController::~HatsNotificationController() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::UmaHistogramEnumeration("Browser.ChromeOS.HatsStatus", state_);

  if (NetworkHandler::IsInitialized())
    NetworkHandler::Get()->network_state_handler()->RemoveObserver(this);
}

void HatsNotificationController::Initialize(bool is_new_device) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (is_new_device && !IsTestingEnabled(hats_config_)) {
    // This device has been chosen for a survey, but it is too new. Instead
    // of showing the user the survey, just mark it as completed.
    UpdateLastInteractionTime();

    state_ = HatsState::kNewDevice;
    return;
  }

  if (NetworkHandler::IsInitialized()) {
    // Observe NetworkStateHandler to be notified when an internet connection
    // is available.
    NetworkStateHandler* handler =
        NetworkHandler::Get()->network_state_handler();
    handler->AddObserver(this);
    // Create an immediate update for the current default network.
    const NetworkState* default_network = handler->DefaultNetwork();
    NetworkState::PortalState portal_state =
        default_network ? default_network->GetPortalState()
                        : NetworkState::PortalState::kUnknown;
    PortalStateChanged(default_network, portal_state);
  }
}

// static
bool HatsNotificationController::ShouldShowSurveyToProfile(
    Profile* profile,
    const HatsConfig& hats_config) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (IsTestingEnabled(hats_config))
    return true;

  // Do not show the survey if the HaTS feature is disabled for the device. This
  // flag is controlled by finch and is enabled only when the device has been
  // selected for the survey.
  if (!base::FeatureList::IsEnabled(hats_config.feature))
    return false;

  // Do not show survey if this is a guest session.
  if (profile->IsGuestSession())
    return false;

  // Do not show survey if the user is supervised.
  if (profile->IsChild())
    return false;

  const bool is_enterprise_enrolled = g_browser_process->platform_part()
                                          ->browser_policy_connector_ash()
                                          ->IsDeviceEnterpriseManaged();

  // Do not show survey to enterprise users.
  if (is_enterprise_enrolled)
    return false;

  // Do not show survey to non-owners.
  if (!ProfileHelper::IsOwnerProfile(profile))
    return false;

  // Call finch helper only after all the profile checks are complete.
  HatsFinchHelper hats_finch_helper(profile, hats_config);
  if (!hats_finch_helper.IsDeviceSelectedForCurrentCycle())
    return false;

  const base::TimeDelta threshold_time = kHatsThreshold;

  // Do not show survey to user if user has interacted with HaTS within the past
  // |threshold_time| time delta.
  if (DidShowSurveyToProfileRecently(profile, threshold_time)) {
    base::UmaHistogramEnumeration("Browser.ChromeOS.HatsStatus",
                                  HatsState::kSurveyShownRecently);
    return false;
  }

  return true;
}

void HatsNotificationController::Click(
    const absl::optional<int>& button_index,
    const absl::optional<std::u16string>& reply) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  UpdateLastInteractionTime();

  std::string user_locale =
      profile_->GetPrefs()->GetString(language::prefs::kApplicationLocale);
  language::ConvertToActualUILocale(&user_locale);
  if (!user_locale.length())
    user_locale = kDefaultProfileLocale;

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&GetFormattedSiteContext, user_locale,
                     product_specific_data_),
      base::BindOnce(&HatsNotificationController::ShowDialog,
                     weak_pointer_factory_.GetWeakPtr()));

  state_ = HatsState::kNotificationClicked;

  // Remove the notification.
  NetworkHandler::Get()->network_state_handler()->RemoveObserver(this);
  notification_.reset(nullptr);
  NotificationDisplayService::GetForProfile(profile_)->Close(
      NotificationHandler::Type::TRANSIENT, kNotificationId);
}

void HatsNotificationController::ShowDialog(const std::string& site_context) {
  if (profile_ != ProfileManager::GetActiveUserProfile()) {
    DVLOG(1) << "Different user detected, not showing dialog";
    return;
  }

  HatsDialog::Show(HatsFinchHelper::GetTriggerID(hats_config_),
                   HatsFinchHelper::GetHistogramName(hats_config_),
                   site_context);
}

// message_center::NotificationDelegate override:
void HatsNotificationController::Close(bool by_user) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (by_user) {
    UpdateLastInteractionTime();
    NetworkHandler::Get()->network_state_handler()->RemoveObserver(this);
    notification_.reset(nullptr);
    state_ = HatsState::kNotificationDismissed;
  }
}

// NetworkStateHandlerObserver override:
void HatsNotificationController::PortalStateChanged(
    const NetworkState* default_network,
    NetworkState::PortalState portal_state) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  VLOG(1) << "PortalStateChanged: default_network="
          << (default_network ? default_network->path() : "")
          << ", portal_state=" << portal_state;
  if (portal_state == NetworkState::PortalState::kOnline) {
    // Create and display the notification for the user.
    if (!notification_) {
      notification_ = CreateSystemNotificationPtr(
          message_center::NOTIFICATION_TYPE_SIMPLE, kNotificationId,
          l10n_util::GetStringUTF16(IDS_HATS_NOTIFICATION_TITLE),
          l10n_util::GetStringUTF16(IDS_HATS_NOTIFICATION_BODY),
          l10n_util::GetStringUTF16(IDS_MESSAGE_CENTER_NOTIFIER_HATS_NAME),
          GURL(kNotificationOriginUrl),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT, kNotifierHats,
              NotificationCatalogName::kHats),
          message_center::RichNotificationData(), this, kNotificationGoogleIcon,
          message_center::SystemNotificationWarningLevel::NORMAL);
    }

    NotificationDisplayService::GetForProfile(profile_)->Display(
        NotificationHandler::Type::TRANSIENT, *notification_,
        /*metadata=*/nullptr);

    state_ = HatsState::kNotificationDisplayed;
  } else if (notification_) {
    // Hide the notification if device loses its connection to the internet.
    NotificationDisplayService::GetForProfile(profile_)->Close(
        NotificationHandler::Type::TRANSIENT, kNotificationId);
  }
}

void HatsNotificationController::OnShuttingDown() {
  NetworkHandler::Get()->network_state_handler()->RemoveObserver(this);
}

// TODO(jackshira): Migrate this to a manager class.
// static
std::string HatsNotificationController::GetFormattedSiteContext(
    const std::string& user_locale,
    const base::flat_map<std::string, std::string>& product_specific_data) {
  base::flat_map<std::string, std::string> context;

  context[KeyEnumToString(DeviceInfoKey::BROWSER)] =
      version_info::GetVersionNumber();

  absl::optional<std::string> version = chromeos::version_loader::GetVersion(
      chromeos::version_loader::VERSION_FULL);
  context[KeyEnumToString(DeviceInfoKey::PLATFORM)] =
      version.value_or("0.0.0.0");

  context[KeyEnumToString(DeviceInfoKey::FIRMWARE)] =
      chromeos::version_loader::GetFirmware();

  context[KeyEnumToString(DeviceInfoKey::LOCALE)] = user_locale;

  for (const auto& pair : context) {
    if (product_specific_data.contains(pair.first)) {
      LOG(WARNING) << "Product specific data contains reserved key "
                   << pair.first << ". Value will be overwritten.";
    }
  }
  context.insert(product_specific_data.begin(), product_specific_data.end());

  std::stringstream stream;
  bool first_iteration = true;
  for (const auto& pair : context) {
    if (!first_iteration)
      stream << kDeviceInfoStopKeyword;

    stream << base::EscapeQueryParamValue(pair.first, /*use_plus=*/false)
           << kDeviceInfoKeyValueDelimiter
           << base::EscapeQueryParamValue(pair.second, /*use_plus=*/false);

    first_iteration = false;
  }
  return stream.str();
}

void HatsNotificationController::UpdateLastInteractionTime() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetInt64(prefs::kHatsLastInteractionTimestamp,
                         base::Time::Now().ToInternalValue());
}

}  // namespace ash
