// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/hats/hats_notification_controller.h"

#include <optional>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/escape.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
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
#include "chrome/browser/notifications/notification_display_service_factory.h"
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
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/strings/grit/ui_strings.h"

namespace ash {

namespace {

const char kNotificationOriginUrl[] = "chrome://hats";

const char kNotifierHats[] = "ash.hats";

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
      NOTREACHED_IN_MIGRATION();
      return std::string();
  }
}

// Returns true if the given `profile` interacted with non-prioritized HaTS
// by either dismissing the notification or taking the survey within a given
// `threshold_time`.
bool DidShowNonPrioritizedHatsToProfileRecently(
    const Profile* profile,
    const base::TimeDelta& threshold_time) {
  int64_t serialized_timestamp =
      profile->GetPrefs()->GetInt64(prefs::kHatsLastInteractionTimestamp);

  base::Time previous_interaction_timestamp =
      base::Time::FromInternalValue(serialized_timestamp);
  return previous_interaction_timestamp + threshold_time > base::Time::Now();
}

// Returns true if the given |profile| interacted with a prioritized HaTS
// by either dismissing the notification or taking another prioritized survey
// within |prioritized_threshold_time|.
// If |hats_config| is given, then also check if the given |profile| interacted
// with that specific prioritized HaTS |hats_config| based on the pref timestamp
// |HatsConfig::survey_last_interaction_timestamp_pref_name| within the
// |HatsConfig::threshold_time|.
bool DidShowPrioritizedHatsToProfileRecently(
    const Profile* profile,
    std::optional<raw_ref<const HatsConfig>> hats_config,
    const base::TimeDelta& prioritized_threshold_time) {
  base::Time prev_prioritized_interaction = profile->GetPrefs()->GetTime(
      prefs::kHatsPrioritizedLastInteractionTimestamp);
  if (prev_prioritized_interaction + prioritized_threshold_time >
      base::Time::Now()) {
    return true;
  }

  if (!hats_config.has_value()) {
    return false;
  }

  base::Time previous_interaction_timestamp = profile->GetPrefs()->GetTime(
      hats_config.value()->survey_last_interaction_timestamp_pref_name);

  return previous_interaction_timestamp + hats_config.value()->threshold_time >
         base::Time::Now();
}

bool DidShowAnyHatsToProfileRecently(const Profile* profile,
                                     const base::TimeDelta& threshold_time) {
  return DidShowNonPrioritizedHatsToProfileRecently(profile, threshold_time) ||
         DidShowPrioritizedHatsToProfileRecently(
             profile, /*hats_config=*/std::nullopt, threshold_time);
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
    const base::flat_map<std::string, std::string>& product_specific_data,
    const std::u16string title,
    const std::u16string body)
    : profile_(profile),
      hats_config_(hats_config),
      product_specific_data_(product_specific_data),
      title_(std::move(title)),
      body_(std::move(body)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::string histogram_name = HatsFinchHelper::GetHistogramName(*hats_config_);
  if (!histogram_name.empty()) {
    base::UmaHistogramSparse(histogram_name, kSurveyTriggeredEnumeration);
  }

  profile_observation_.Observe(profile_);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&IsNewDevice, hats_config.new_device_threshold),
      base::BindOnce(&HatsNotificationController::Initialize,
                     weak_pointer_factory_.GetWeakPtr()));
}

HatsNotificationController::HatsNotificationController(
    Profile* profile,
    const HatsConfig& hats_config,
    const base::flat_map<std::string, std::string>& product_specific_data)
    : HatsNotificationController(
          profile,
          hats_config,
          product_specific_data,
          l10n_util::GetStringUTF16(IDS_HATS_NOTIFICATION_TITLE),
          l10n_util::GetStringUTF16(IDS_HATS_NOTIFICATION_BODY)) {}

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

  if (is_new_device && !IsTestingEnabled(*hats_config_)) {
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

  HatsFinchHelper hats_finch_helper(profile, hats_config);

  // Do not show survey to enterprise users.
  // Exceptions for Googlers if the survey wants Googlers participation.
  if (is_enterprise_enrolled &&
      !(gaia::IsGoogleInternalAccountEmail(profile->GetProfileUserName()) &&
        hats_finch_helper.IsEnabledForGooglers(hats_config))) {
    return false;
  }

  // Do not show survey to non-owners. However, enterprise-enrolled Googlers
  // who passed the previous check will not be owners; don't exclude them.
  if (!is_enterprise_enrolled && !ProfileHelper::IsOwnerProfile(profile)) {
    return false;
  }

  if (!hats_finch_helper.IsDeviceSelectedForCurrentCycle())
    return false;

  // There are two types of HaTS: prioritized and the non prioritized,
  // both are kept track separately. The following checks both track records.
  if (DidShowAnyHatsToProfileRecently(profile, kMinimumHatsThreshold)) {
    return false;
  }

  if (hats_config.prioritized) {
    // Do not show survey to user if the survey is prioritized and:
    // - User already interacted with the survey within
    //   the threshold set in the config, or
    // - User already interacted with other prioritized survey within
    //   the past |kPrioritizedHatsThreshold|.
    if (DidShowPrioritizedHatsToProfileRecently(
            profile, raw_ref<const HatsConfig>(hats_config),
            kPrioritizedHatsThreshold)) {
      return false;
    }
  } else {
    const base::TimeDelta threshold_time =
        gaia::IsGoogleInternalAccountEmail(profile->GetProfileUserName())
            ? kHatsGooglerThreshold
            : kHatsThreshold;
    // Do not show survey to user if user has interacted with HaTS within the
    // past |threshold_time| time delta. This is a global cap applied across
    // surveys that have not opted out of the global cap of 1 per kHatsThreshold
    // days.
    if (DidShowNonPrioritizedHatsToProfileRecently(profile, threshold_time)) {
      base::UmaHistogramEnumeration("Browser.ChromeOS.HatsStatus",
                                    HatsState::kSurveyShownRecently);
      return false;
    }
  }
  return true;
}

void HatsNotificationController::Click(
    const std::optional<int>& button_index,
    const std::optional<std::u16string>& reply) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(profile_) << "Profile must NOT be null.";

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
  NotificationDisplayServiceFactory::GetForProfile(profile_)->Close(
      NotificationHandler::Type::TRANSIENT, kNotificationId);
}

void HatsNotificationController::ShowDialog(const std::string& site_context) {
  if (profile_ != ProfileManager::GetActiveUserProfile()) {
    DVLOG(1) << "Different user detected, not showing dialog";
    return;
  }

  HatsDialog::Show(HatsFinchHelper::GetTriggerID(*hats_config_),
                   HatsFinchHelper::GetHistogramName(*hats_config_),
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
  CHECK(profile_) << "Profile must NOT be null.";
  VLOG(1) << "PortalStateChanged: default_network="
          << (default_network ? default_network->path() : "")
          << ", portal_state=" << portal_state;
  if (portal_state == NetworkState::PortalState::kOnline) {
    // Create and display the notification for the user.
    if (!notification_) {
      notification_ = CreateSystemNotificationPtr(
          message_center::NOTIFICATION_TYPE_SIMPLE, kNotificationId, title_,
          body_,
          l10n_util::GetStringUTF16(IDS_MESSAGE_CENTER_NOTIFIER_HATS_NAME),
          GURL(kNotificationOriginUrl),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT, kNotifierHats,
              NotificationCatalogName::kHats),
          message_center::RichNotificationData(), this, kNotificationGoogleIcon,
          message_center::SystemNotificationWarningLevel::NORMAL);
    }

    NotificationDisplayServiceFactory::GetForProfile(profile_)->Display(
        NotificationHandler::Type::TRANSIENT, *notification_,
        /*metadata=*/nullptr);

    state_ = HatsState::kNotificationDisplayed;
  } else if (notification_) {
    // Hide the notification if device loses its connection to the internet.
    NotificationDisplayServiceFactory::GetForProfile(profile_)->Close(
        NotificationHandler::Type::TRANSIENT, kNotificationId);
  }
}

void HatsNotificationController::OnShuttingDown() {
  NetworkHandler::Get()->network_state_handler()->RemoveObserver(this);
}

void HatsNotificationController::OnProfileWillBeDestroyed(Profile* profile) {
  CHECK_EQ(profile_, profile);
  profile_ = nullptr;
  profile_observation_.Reset();
}

// TODO(jackshira): Migrate this to a manager class.
// static
std::string HatsNotificationController::GetFormattedSiteContext(
    const std::string& user_locale,
    const base::flat_map<std::string, std::string>& product_specific_data) {
  base::flat_map<std::string, std::string> context;

  context[KeyEnumToString(DeviceInfoKey::BROWSER)] =
      version_info::GetVersionNumber();

  std::optional<std::string> version = chromeos::version_loader::GetVersion(
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
  CHECK(profile_) << "Profile must NOT be null.";

  PrefService* pref_service = profile_->GetPrefs();
  if (!hats_config_->prioritized) {
    pref_service->SetInt64(prefs::kHatsLastInteractionTimestamp,
                           base::Time::Now().since_origin().InMicroseconds());
  } else {
    pref_service->SetTime(
        hats_config_->survey_last_interaction_timestamp_pref_name,
        base::Time::Now());
    pref_service->SetTime(prefs::kHatsPrioritizedLastInteractionTimestamp,
                           base::Time::Now());
  }
}

}  // namespace ash
