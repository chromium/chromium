// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/arc_support_host.h"

#include <string>
#include <utility>
#include <vector>

#include "ash/components/arc/app/arc_app_constants.h"
#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/hash/sha1.h"
#include "base/i18n/timezone.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/consent_auditor/consent_auditor_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/webui/ash/diagnostics_dialog/diagnostics_dialog.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/consent_auditor/consent_auditor.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/extension_registry.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/native_widget_types.h"

using sync_pb::UserConsentTypes;

namespace {
constexpr char kAction[] = "action";
constexpr char kArcManaged[] = "arcManaged";
constexpr char kData[] = "data";
constexpr char kDeviceId[] = "deviceId";
constexpr char kActionInitialize[] = "initialize";
constexpr char kActionSetMetricsMode[] = "setMetricsMode";
constexpr char kActionBackupAndRestoreMode[] = "setBackupAndRestoreMode";
constexpr char kActionLocationServiceMode[] = "setLocationServiceMode";
constexpr char kActionSetWindowBounds[] = "setWindowBounds";
constexpr char kActionCloseWindow[] = "closeWindow";

// Action to show a page. The message should have "page" field, which is one of
// IDs for section div elements.
constexpr char kActionShowPage[] = "showPage";
constexpr char kPage[] = "page";

// Action to show the error page. The message should have "errorMessage",
// which is a localized error text, and "shouldShowSendFeedback" boolean value.
constexpr char kActionShowErrorPage[] = "showErrorPage";
constexpr char kErrorMessage[] = "errorMessage";
constexpr char kShouldShowSendFeedback[] = "shouldShowSendFeedback";
constexpr char kShouldShowNetworkTests[] = "shouldShowNetworkTests";

// The preference update should have those two fields.
constexpr char kEnabled[] = "enabled";
constexpr char kManaged[] = "managed";

// The JSON data sent from the extension should have at least "event" field.
// Each event data is defined below.
// The key of the event type.
constexpr char kEvent[] = "event";

// "onWindowClosed" is fired when the extension window is closed.
// No data will be provided.
constexpr char kEventOnWindowClosed[] = "onWindowClosed";

// "onAgreed" is fired when a user clicks "Agree" button.
// The message should have the following fields:
// - tosContent
// - tosShown
// - isMetricsEnabled
// - isBackupRestoreEnabled
// - isBackupRestoreManaged
// - isLocationServiceEnabled
// - isLocationServiceManaged
constexpr char kEventOnAgreed[] = "onAgreed";
constexpr char kTosContent[] = "tosContent";
constexpr char kTosShown[] = "tosShown";
constexpr char kIsMetricsEnabled[] = "isMetricsEnabled";
constexpr char kIsBackupRestoreEnabled[] = "isBackupRestoreEnabled";
constexpr char kIsBackupRestoreManaged[] = "isBackupRestoreManaged";
constexpr char kIsLocationServiceEnabled[] = "isLocationServiceEnabled";
constexpr char kIsLocationServiceManaged[] = "isLocationServiceManaged";

// "onCanceled" is fired when user clicks "Cancel" button.
// The message should have the same fields as "onAgreed" above.
constexpr char kEventOnCanceled[] = "onCanceled";

// "onRetryClicked" is fired when a user clicks "RETRY" button on the error
// page.
constexpr char kEventOnRetryClicked[] = "onRetryClicked";

// "onSendFeedbackClicked" is fired when a user clicks "Send Feedback" button.
constexpr char kEventOnSendFeedbackClicked[] = "onSendFeedbackClicked";

// "onRunNetworkTestsClicked" is fired when a user clicks "Check Network"
// button.
constexpr char kEventOnRunNetworkTestsClicked[] = "onRunNetworkTestsClicked";

// "onTosLoadResult" is fired when terms of service page is loaded or fails to
// load.
constexpr char kEventOnTosLoadResult[] = "onTosLoadResult";

// "onTosLoadResult" should have the following fields:
// - success
constexpr char kSuccess[] = "success";

// "onErrorPageShown" is fired when the error page is shown.
constexpr char kEventOnErrorPageShown[] = "onErrorPageShown";

// "OnErrorPageShown" should have the following fields:
// - networkTestsShown
constexpr char kNetworkTestsShown[] = "networkTestsShown";

// "onOpenPrivacySettingsPageClicked" is fired when a user clicks privacy
// settings link.
constexpr char kEventOnOpenPrivacySettingsPageClicked[] =
    "onOpenPrivacySettingsPageClicked";

// "requestWindowBound" is fired when new opt-in window is created.
constexpr char kEventRequestWindowBounds[] = "requestWindowBounds";

// x,y,width,height to define current work area.
constexpr char kDisplayWorkareaX[] = "displayWorkareaX";
constexpr char kDisplayWorkareaY[] = "displayWorkareaY";
constexpr char kDisplayWorkareaWidth[] = "displayWorkareaWidth";
constexpr char kDisplayWorkareaHeight[] = "displayWorkareaHeight";

void RequestOpenApp(Profile* profile) {
  apps::AppServiceProxyFactory::GetForProfile(profile)
      ->BrowserAppLauncher()
      ->LaunchPlayStoreWithExtensions();
}

std::ostream& operator<<(std::ostream& os, ArcSupportHost::UIPage ui_page) {
  switch (ui_page) {
    case ArcSupportHost::UIPage::NO_PAGE:
      return os << "NO_PAGE";
    case ArcSupportHost::UIPage::TERMS:
      return os << "TERMS";
    case ArcSupportHost::UIPage::ARC_LOADING:
      return os << "ARC_LOADING";
    case ArcSupportHost::UIPage::ERROR:
      return os << "ERROR";
  }

  // Some compiler reports an error even if all values of an enum-class are
  // covered individually in a switch statement.
  NOTREACHED_IN_MIGRATION();
  return os;
}

std::ostream& operator<<(std::ostream& os, ArcSupportHost::Error error) {
#define MAP_ERROR(name)             \
  case ArcSupportHost::Error::name: \
    return os << #name
  switch (error) {
    MAP_ERROR(SIGN_IN_NETWORK_ERROR);
    MAP_ERROR(SIGN_IN_GMS_SIGNIN_ERROR);
    MAP_ERROR(SIGN_IN_BAD_AUTHENTICATION_ERROR);
    MAP_ERROR(SIGN_IN_GMS_CHECKIN_ERROR);
    MAP_ERROR(SIGN_IN_CLOUD_PROVISION_FLOW_ACCOUNT_MISSING_ERROR);
    MAP_ERROR(SIGN_IN_CLOUD_PROVISION_FLOW_DOMAIN_JOIN_FAIL_ERROR);
    MAP_ERROR(SIGN_IN_CLOUD_PROVISION_FLOW_ENROLLMENT_TOKEN_INVALID);
    MAP_ERROR(SIGN_IN_CLOUD_PROVISION_FLOW_INTERRUPTED_ERROR);
    MAP_ERROR(SIGN_IN_CLOUD_PROVISION_FLOW_NETWORK_ERROR);
    MAP_ERROR(SIGN_IN_CLOUD_PROVISION_FLOW_PERMANENT_ERROR);
    MAP_ERROR(SIGN_IN_CLOUD_PROVISION_FLOW_TRANSIENT_ERROR);
    MAP_ERROR(SIGN_IN_UNKNOWN_ERROR);
    MAP_ERROR(SERVER_COMMUNICATION_ERROR);
    MAP_ERROR(ANDROID_MANAGEMENT_REQUIRED_ERROR);
    MAP_ERROR(NETWORK_UNAVAILABLE_ERROR);
    MAP_ERROR(LOW_DISK_SPACE_ERROR);
  }
#undef MAP_ERROR

  // Some compiler reports an error even if all values of an enum-class are
  // covered individually in a switch statement.
  NOTREACHED_IN_MIGRATION();
  return os;
}

}  // namespace

ArcSupportHost::ErrorInfo::ErrorInfo(Error error)
    : error(error), arg(std::nullopt) {}
ArcSupportHost::ErrorInfo::ErrorInfo(Error error, const std::optional<int>& arg)
    : error(error), arg(arg) {}
ArcSupportHost::ErrorInfo::ErrorInfo(const ErrorInfo&) = default;
ArcSupportHost::ErrorInfo& ArcSupportHost::ErrorInfo::operator=(
    const ArcSupportHost::ErrorInfo&) = default;

ArcSupportHost::ArcSupportHost(Profile* profile)
    : profile_(profile),
      request_open_app_callback_(base::BindRepeating(&RequestOpenApp)) {
  DCHECK(profile_);
}

ArcSupportHost::~ArcSupportHost() {
  // Delegates should have been reset to nullptr at this point.
  DCHECK(!tos_delegate_);
  DCHECK(!error_delegate_);

  if (message_host_)
    DisconnectMessageHost();
}

void ArcSupportHost::SetTermsOfServiceDelegate(
    TermsOfServiceDelegate* delegate) {
  tos_delegate_ = delegate;
}

void ArcSupportHost::SetErrorDelegate(ErrorDelegate* delegate) {
  error_delegate_ = delegate;
}

gfx::NativeWindow ArcSupportHost::GetNativeWindow() const {
  extensions::AppWindowRegistry* registry =
      extensions::AppWindowRegistry::Get(profile_);
  if (!registry) {
    return gfx::NativeWindow();
  }

  extensions::AppWindow* window =
      registry->GetCurrentAppWindowForApp(arc::kPlayStoreAppId);
  return window ? window->GetNativeWindow() : gfx::NativeWindow();
}

bool ArcSupportHost::GetShouldShowRunNetworkTests() {
  return should_show_run_network_tests_;
}

void ArcSupportHost::SetArcManaged(bool is_arc_managed) {
  DCHECK(!message_host_ || (is_arc_managed_ == is_arc_managed));
  is_arc_managed_ = is_arc_managed;
}

void ArcSupportHost::Close() {
  ui_page_ = UIPage::NO_PAGE;
  if (!message_host_) {
    VLOG(2) << "ArcSupportHost::Close() is called "
            << "but message_host_ is not available.";
    return;
  }

  base::Value::Dict message;
  message.Set(kAction, kActionCloseWindow);
  message_host_->SendMessage(message);

  // Disconnect immediately, so that onWindowClosed event will not be
  // delivered to here.
  DisconnectMessageHost();
}

void ArcSupportHost::ShowTermsOfService() {
  ShowPage(UIPage::TERMS);
}

void ArcSupportHost::ShowArcLoading() {
  ShowPage(UIPage::ARC_LOADING);
}

void ArcSupportHost::ShowPage(UIPage ui_page) {
  ui_page_ = ui_page;
  if (!message_host_) {
    if (app_start_pending_) {
      VLOG(2) << "ArcSupportHost::ShowPage(" << ui_page << ") is called "
              << "before connection to ARC support Chrome app has finished "
              << "establishing.";
      return;
    }
    RequestAppStart();
    return;
  }

  base::Value::Dict message;
  message.Set(kAction, kActionShowPage);
  switch (ui_page) {
    case UIPage::TERMS:
      message.Set(kPage, "terms");
      break;
    case UIPage::ARC_LOADING:
      message.Set(kPage, "arc-loading");
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      return;
  }
  message_host_->SendMessage(message);
}

void ArcSupportHost::ShowError(ErrorInfo error_info,
                               bool should_show_send_feedback,
                               bool should_show_run_network_tests) {
  ui_page_ = UIPage::ERROR;
  error_info_.emplace(error_info);
  should_show_send_feedback_ = should_show_send_feedback;
  should_show_run_network_tests_ = should_show_run_network_tests;
  if (!message_host_) {
    if (app_start_pending_) {
      VLOG(2) << "ArcSupportHost::ShowError(" << error_info.error << ", "
              << error_info.arg.value_or(-1) << ", "
              << should_show_send_feedback << ", "
              << should_show_run_network_tests
              << ") is called before connection "
              << "to ARC support Chrome app is established.";
      return;
    }
    RequestAppStart();
    return;
  }

  base::Value::Dict message_args;
  message_args.Set(kAction, kActionShowErrorPage);
  int message_id;
#define MAP_ERROR(name, id) \
  case Error::name:         \
    message_id = id;        \
    break
  switch (error_info.error) {
    MAP_ERROR(SIGN_IN_NETWORK_ERROR, IDS_ARC_SIGN_IN_NETWORK_ERROR);
    MAP_ERROR(SIGN_IN_GMS_SIGNIN_ERROR, IDS_ARC_SIGN_IN_GMS_SIGNIN_ERROR);
    MAP_ERROR(SIGN_IN_BAD_AUTHENTICATION_ERROR,
              IDS_ARC_SIGN_IN_BAD_AUTHENTICATION_ERROR);
    MAP_ERROR(SIGN_IN_GMS_CHECKIN_ERROR, IDS_ARC_SIGN_IN_GMS_CHECKIN_ERROR);
    MAP_ERROR(SIGN_IN_CLOUD_PROVISION_FLOW_ACCOUNT_MISSING_ERROR,
              IDS_ARC_SIGN_IN_CLOUD_PROVISION_FLOW_ACCOUNT_MISSING_ERROR);
    MAP_ERROR(SIGN_IN_CLOUD_PROVISION_FLOW_DOMAIN_JOIN_FAIL_ERROR,
              IDS_ARC_SIGN_IN_CLOUD_PROVISION_FLOW_DOMAIN_JOIN_FAIL_ERROR);
    MAP_ERROR(SIGN_IN_CLOUD_PROVISION_FLOW_ENROLLMENT_TOKEN_INVALID,
              IDS_ARC_SIGN_IN_CLOUD_PROVISION_FLOW_ENROLLMENT_TOKEN_INVALID);
    MAP_ERROR(SIGN_IN_CLOUD_PROVISION_FLOW_INTERRUPTED_ERROR,
              IDS_ARC_SIGN_IN_CLOUD_PROVISION_FLOW_INTERRUPTED_ERROR);
    MAP_ERROR(SIGN_IN_CLOUD_PROVISION_FLOW_NETWORK_ERROR,
              IDS_ARC_SIGN_IN_CLOUD_PROVISION_FLOW_NETWORK_ERROR);
    MAP_ERROR(SIGN_IN_CLOUD_PROVISION_FLOW_PERMANENT_ERROR,
              IDS_ARC_SIGN_IN_CLOUD_PROVISION_FLOW_PERMANENT_ERROR);
    MAP_ERROR(SIGN_IN_CLOUD_PROVISION_FLOW_TRANSIENT_ERROR,
              IDS_ARC_SIGN_IN_CLOUD_PROVISION_FLOW_TRANSIENT_ERROR);
    MAP_ERROR(SIGN_IN_UNKNOWN_ERROR, IDS_ARC_SIGN_IN_UNKNOWN_ERROR);
    MAP_ERROR(SERVER_COMMUNICATION_ERROR, IDS_ARC_SERVER_COMMUNICATION_ERROR);
    MAP_ERROR(ANDROID_MANAGEMENT_REQUIRED_ERROR,
              IDS_ARC_ANDROID_MANAGEMENT_REQUIRED_ERROR);
    MAP_ERROR(NETWORK_UNAVAILABLE_ERROR, IDS_ARC_NETWORK_UNAVAILABLE_ERROR);
    MAP_ERROR(LOW_DISK_SPACE_ERROR, IDS_ARC_LOW_DISK_SPACE_ERROR);
  }
#undef MAP_ERROR

  std::u16string message;
  switch (error_info.error) {
    case Error::SIGN_IN_CLOUD_PROVISION_FLOW_ACCOUNT_MISSING_ERROR:
    case Error::SIGN_IN_CLOUD_PROVISION_FLOW_DOMAIN_JOIN_FAIL_ERROR:
    case Error::SIGN_IN_CLOUD_PROVISION_FLOW_ENROLLMENT_TOKEN_INVALID:
    case Error::SIGN_IN_CLOUD_PROVISION_FLOW_INTERRUPTED_ERROR:
    case Error::SIGN_IN_CLOUD_PROVISION_FLOW_NETWORK_ERROR:
    case Error::SIGN_IN_CLOUD_PROVISION_FLOW_PERMANENT_ERROR:
    case Error::SIGN_IN_CLOUD_PROVISION_FLOW_TRANSIENT_ERROR:
    case Error::SIGN_IN_GMS_SIGNIN_ERROR:
    case Error::SIGN_IN_GMS_CHECKIN_ERROR:
    case Error::SIGN_IN_UNKNOWN_ERROR:
      DCHECK(error_info.arg);
      message = l10n_util::GetStringFUTF16(
          message_id, base::NumberToString16(error_info.arg.value()));
      break;
    default:
      message = l10n_util::GetStringUTF16(message_id);
      break;
  }

  message_args.Set(kErrorMessage, message);
  message_args.Set(kShouldShowSendFeedback, should_show_send_feedback);
  message_args.Set(kShouldShowNetworkTests, should_show_run_network_tests);
  message_host_->SendMessage(message_args);
}

void ArcSupportHost::SetMetricsPreferenceCheckbox(bool is_enabled,
                                                  bool is_managed) {
  metrics_checkbox_ = PreferenceCheckboxData(is_enabled, is_managed);
  SendPreferenceCheckboxUpdate(kActionSetMetricsMode, metrics_checkbox_);
}

void ArcSupportHost::SetBackupAndRestorePreferenceCheckbox(bool is_enabled,
                                                           bool is_managed) {
  backup_and_restore_checkbox_ = PreferenceCheckboxData(is_enabled, is_managed);
  SendPreferenceCheckboxUpdate(kActionBackupAndRestoreMode,
                               backup_and_restore_checkbox_);
}

void ArcSupportHost::SetLocationServicesPreferenceCheckbox(bool is_enabled,
                                                           bool is_managed) {
  location_services_checkbox_ = PreferenceCheckboxData(is_enabled, is_managed);
  SendPreferenceCheckboxUpdate(kActionLocationServiceMode,
                               location_services_checkbox_);
}

void ArcSupportHost::SendPreferenceCheckboxUpdate(
    const std::string& action_name,
    const PreferenceCheckboxData& data) {
  if (!message_host_)
    return;

  base::Value::Dict message;
  message.Set(kAction, action_name);
  message.Set(kEnabled, data.is_enabled);
  message.Set(kManaged, data.is_managed);
  message_host_->SendMessage(message);
}

void ArcSupportHost::SetMessageHost(arc::ArcSupportMessageHost* message_host) {
  if (message_host_ == message_host)
    return;

  app_start_pending_ = false;
  if (message_host_)
    DisconnectMessageHost();
  message_host_ = message_host;
  message_host_->SetObserver(this);
  display_observer_.emplace(this);

  if (!Initialize()) {
    Close();
    return;
  }

  // Hereafter, install the requested ui state into the ARC support Chrome app.

  // Set preference checkbox state.
  SendPreferenceCheckboxUpdate(kActionSetMetricsMode, metrics_checkbox_);
  SendPreferenceCheckboxUpdate(kActionBackupAndRestoreMode,
                               backup_and_restore_checkbox_);
  SendPreferenceCheckboxUpdate(kActionLocationServiceMode,
                               location_services_checkbox_);

  if (ui_page_ == UIPage::NO_PAGE) {
    // Close() is called before opening the window.
    Close();
  } else if (ui_page_ == UIPage::ERROR) {
    DCHECK(error_info_);
    ShowError(error_info_.value(), should_show_send_feedback_,
              should_show_run_network_tests_);
  } else {
    ShowPage(ui_page_);
  }
}

void ArcSupportHost::UnsetMessageHost(
    arc::ArcSupportMessageHost* message_host) {
  if (message_host_ != message_host)
    return;
  DisconnectMessageHost();
}

void ArcSupportHost::DisconnectMessageHost() {
  DCHECK(message_host_);
  display_observer_.reset();
  message_host_->SetObserver(nullptr);
  message_host_ = nullptr;
}

void ArcSupportHost::RequestAppStart() {
  DCHECK(!message_host_);
  DCHECK(!app_start_pending_);

  app_start_pending_ = true;
  request_open_app_callback_.Run(profile_.get());
}

void ArcSupportHost::SetRequestOpenAppCallbackForTesting(
    const RequestOpenAppCallback& callback) {
  DCHECK(!message_host_);
  DCHECK(!app_start_pending_);
  DCHECK(!callback.is_null());
  request_open_app_callback_ = callback;
}

bool ArcSupportHost::Initialize() {
  DCHECK(message_host_);

  const bool is_child =
      user_manager::UserManager::Get()->IsLoggedInAsChildUser();

  base::Value::Dict loadtime_data;
  loadtime_data.Set("appWindow", l10n_util::GetStringUTF16(
                                     IDS_ARC_PLAYSTORE_ICON_TITLE_BETA));
  loadtime_data.Set("greetingHeader",
                    l10n_util::GetStringUTF16(IDS_ARC_OOBE_TERMS_HEADING));
  loadtime_data.Set(
      "initializingHeader",
      l10n_util::GetStringUTF16(IDS_ARC_PLAYSTORE_SETTING_UP_TITLE));
  loadtime_data.Set("greetingDescription",
                    l10n_util::GetStringUTF16(IDS_ARC_OOBE_TERMS_DESCRIPTION));
  loadtime_data.Set("buttonAgree", l10n_util::GetStringUTF16(
                                       IDS_ARC_OPT_IN_DIALOG_BUTTON_AGREE));
  loadtime_data.Set("buttonCancel", l10n_util::GetStringUTF16(
                                        IDS_ARC_OPT_IN_DIALOG_BUTTON_CANCEL));
  loadtime_data.Set("buttonNext", l10n_util::GetStringUTF16(
                                      IDS_ARC_OPT_IN_DIALOG_BUTTON_NEXT));
  loadtime_data.Set(
      "buttonSendFeedback",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_DIALOG_BUTTON_SEND_FEEDBACK));
  loadtime_data.Set("buttonRunNetworkTests",
                    l10n_util::GetStringUTF16(
                        IDS_ARC_OPT_IN_DIALOG_BUTTON_RUN_NETWORK_TESTS));
  loadtime_data.Set("buttonRetry", l10n_util::GetStringUTF16(
                                       IDS_ARC_OPT_IN_DIALOG_BUTTON_RETRY));
  loadtime_data.Set(
      "progressTermsLoading",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_DIALOG_PROGRESS_TERMS));
  loadtime_data.Set(
      "progressAndroidLoading",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_DIALOG_PROGRESS_ANDROID));
  loadtime_data.Set(
      "authorizationFailed",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_DIALOG_AUTHORIZATION_FAILED));
  loadtime_data.Set(
      "termsOfService",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_DIALOG_TERMS_OF_SERVICE));
  loadtime_data.Set("textMetricsEnabled",
                    l10n_util::GetStringUTF16(
                        is_child ? IDS_ARC_OPT_IN_DIALOG_METRICS_ENABLED_CHILD
                                 : IDS_ARC_OPT_IN_DIALOG_METRICS_ENABLED));
  loadtime_data.Set("textMetricsDisabled",
                    l10n_util::GetStringUTF16(
                        is_child ? IDS_ARC_OPT_IN_DIALOG_METRICS_DISABLED_CHILD
                                 : IDS_ARC_OPT_IN_DIALOG_METRICS_DISABLED));
  loadtime_data.Set(
      "textMetricsManagedEnabled",
      l10n_util::GetStringUTF16(
          is_child ? IDS_ARC_OPT_IN_DIALOG_METRICS_MANAGED_ENABLED_CHILD
                   : IDS_ARC_OPT_IN_DIALOG_METRICS_MANAGED_ENABLED));
  loadtime_data.Set(
      "textMetricsManagedDisabled",
      l10n_util::GetStringUTF16(
          is_child ? IDS_ARC_OPT_IN_DIALOG_METRICS_MANAGED_DISABLED_CHILD
                   : IDS_ARC_OPT_IN_DIALOG_METRICS_MANAGED_DISABLED));
  loadtime_data.Set(
      "textBackupRestoreLabel",
      l10n_util::GetStringUTF16(
          is_child ? IDS_ARC_OPT_IN_DIALOG_BACKUP_RESTORE_CHILD_LABEL
                   : IDS_ARC_OPT_IN_DIALOG_BACKUP_RESTORE_LABEL));
  loadtime_data.Set("textBackupRestore",
                    l10n_util::GetStringUTF16(
                        is_child ? IDS_ARC_OPT_IN_DIALOG_BACKUP_RESTORE_CHILD
                                 : IDS_ARC_OPT_IN_DIALOG_BACKUP_RESTORE));
  loadtime_data.Set("textPaiService",
                    l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_PAI));
  loadtime_data.Set(
      "textGoogleServiceConfirmation",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_GOOGLE_SERVICE_CONFIRMATION));
  if (ash::features::IsCrosPrivacyHubLocationEnabled()) {
    loadtime_data.Set("textLocationService",
                      l10n_util::GetStringUTF16(
                          is_child ? IDS_CROS_OPT_IN_LOCATION_SETTING_CHILD
                                   : IDS_CROS_OPT_IN_LOCATION_SETTING));
  } else {
    loadtime_data.Set("textLocationService",
                      l10n_util::GetStringUTF16(
                          is_child ? IDS_ARC_OPT_IN_LOCATION_SETTING_CHILD
                                   : IDS_ARC_OPT_IN_LOCATION_SETTING));
  }
  loadtime_data.Set("serverError", l10n_util::GetStringUTF16(
                                       IDS_ARC_SERVER_COMMUNICATION_ERROR));
  loadtime_data.Set("controlledByPolicy",
                    l10n_util::GetStringUTF16(IDS_CONTROLLED_SETTING_POLICY));
  loadtime_data.Set(
      "learnMoreStatisticsTitle",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_LEARN_MORE_STATISTICS_TITLE));
  loadtime_data.Set("learnMoreStatistics",
                    l10n_util::GetStringUTF16(
                        is_child ? IDS_ARC_OPT_IN_LEARN_MORE_STATISTICS_CHILD
                                 : IDS_ARC_OPT_IN_LEARN_MORE_STATISTICS));
  loadtime_data.Set("learnMoreBackupAndRestoreTitle",
                    l10n_util::GetStringUTF16(
                        IDS_ARC_OPT_IN_LEARN_MORE_BACKUP_AND_RESTORE_TITLE));
  loadtime_data.Set(
      "learnMoreBackupAndRestore",
      l10n_util::GetStringUTF16(
          is_child ? IDS_ARC_OPT_IN_LEARN_MORE_BACKUP_AND_RESTORE_CHILD
                   : IDS_ARC_OPT_IN_LEARN_MORE_BACKUP_AND_RESTORE));
  loadtime_data.Set("learnMoreLocationServicesTitle",
                    l10n_util::GetStringUTF16(
                        IDS_ARC_OPT_IN_LEARN_MORE_LOCATION_SERVICES_TITLE));
  if (ash::features::IsCrosPrivacyHubLocationEnabled()) {
    loadtime_data.Set(
        "learnMoreLocationServices",
        l10n_util::GetStringFUTF16(
            is_child ? IDS_CROS_OPT_IN_LEARN_MORE_LOCATION_SERVICES_CHILD
                     : IDS_CROS_OPT_IN_LEARN_MORE_LOCATION_SERVICES,
            chrome::kPrivacyHubGeolocationAccuracyLearnMoreURL));
  } else {
    loadtime_data.Set(
        "learnMoreLocationServices",
        l10n_util::GetStringUTF16(
            is_child ? IDS_ARC_OPT_IN_LEARN_MORE_LOCATION_SERVICES_CHILD
                     : IDS_ARC_OPT_IN_LEARN_MORE_LOCATION_SERVICES));
  }
  loadtime_data.Set(
      "learnMorePaiServiceTitle",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_LEARN_MORE_PAI_SERVICE_TITLE));
  loadtime_data.Set(
      "learnMorePaiService",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_LEARN_MORE_PAI_SERVICE));
  loadtime_data.Set("overlayClose",
                    l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_LEARN_MORE_CLOSE));
  loadtime_data.Set(
      "privacyPolicyLink",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_PRIVACY_POLICY_LINK));
  loadtime_data.Set("overlayLoading",
                    l10n_util::GetStringUTF16(IDS_ARC_POPUP_HELP_LOADING));

  loadtime_data.Set(kArcManaged, is_arc_managed_);
  loadtime_data.Set("isOwnerProfile",
                    ash::ProfileHelper::IsOwnerProfile(profile_));

  const std::string& country_code = base::CountryCodeForCurrentTimezone();
  loadtime_data.Set("countryCode", country_code);

  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  webui::SetLoadTimeDataDefaults(app_locale, &loadtime_data);
  loadtime_data.Set("locale", app_locale);

  base::Value::Dict message;
  message.Set(kAction, kActionInitialize);
  message.Set(kData, std::move(loadtime_data));

  user_manager::KnownUser known_user(g_browser_process->local_state());
  const std::string device_id = known_user.GetDeviceId(
      multi_user_util::GetAccountIdFromProfile(profile_));
  message.Set(kDeviceId, device_id);

  message_host_->SendMessage(message);
  return true;
}

void ArcSupportHost::OnDisplayMetricsChanged(const display::Display& display,
                                             uint32_t changed_metrics) {
  SetWindowBound(display);
}

void ArcSupportHost::SetWindowBound(const display::Display& display) {
  if (!message_host_)
    return;

  base::Value::Dict message;
  message.Set(kAction, kActionSetWindowBounds);
  message.Set(kDisplayWorkareaX, display.work_area().x());
  message.Set(kDisplayWorkareaY, display.work_area().y());
  message.Set(kDisplayWorkareaWidth, display.work_area().width());
  message.Set(kDisplayWorkareaHeight, display.work_area().height());
  message_host_->SendMessage(message);
}

void ArcSupportHost::OnMessage(const base::Value::Dict& message) {
  const std::string* event = message.FindString(kEvent);
  if (!event) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  if (*event == kEventOnWindowClosed) {
    // If ToS negotiation is ongoing, call the specific function.
    if (tos_delegate_) {
      tos_delegate_->OnTermsRejected();
    } else {
      DCHECK(error_delegate_);
      error_delegate_->OnWindowClosed();
    }
  } else if (*event == kEventOnAgreed || *event == kEventOnCanceled) {
    DCHECK(tos_delegate_);
    std::optional<bool> tos_shown = message.FindBool(kTosShown);
    std::optional<bool> is_metrics_enabled =
        message.FindBool(kIsMetricsEnabled);
    std::optional<bool> is_backup_restore_enabled =
        message.FindBool(kIsBackupRestoreEnabled);
    std::optional<bool> is_backup_restore_managed =
        message.FindBool(kIsBackupRestoreManaged);
    std::optional<bool> is_location_service_enabled =
        message.FindBool(kIsLocationServiceEnabled);
    std::optional<bool> is_location_service_managed =
        message.FindBool(kIsLocationServiceManaged);

    const std::string* tos_content = message.FindString(kTosContent);
    if (!tos_content || !tos_shown.has_value() ||
        !is_metrics_enabled.has_value() ||
        !is_backup_restore_enabled.has_value() ||
        !is_backup_restore_managed.has_value() ||
        !is_location_service_enabled.has_value() ||
        !is_location_service_managed.has_value()) {
      NOTREACHED_IN_MIGRATION();
      return;
    }

    bool accepted = *event == kEventOnAgreed;
    if (!accepted) {
      // Cancel is equivalent to not granting consent to the individual
      // features, so ensure we don't record consent.
      is_backup_restore_enabled = false;
      is_location_service_enabled = false;
    }

    auto* identity_manager = IdentityManagerFactory::GetForProfile(profile_);
    // This class doesn't care about browser sync consent.
    DCHECK(identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));
    CoreAccountId account_id =
        identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
    bool is_child = user_manager::UserManager::Get()->IsLoggedInAsChildUser();

    // Record acceptance of ToS if it was shown to the user, otherwise simply
    // record acceptance of an empty ToS.
    UserConsentTypes::ArcPlayTermsOfServiceConsent play_consent;
    play_consent.set_status(accepted ? UserConsentTypes::GIVEN
                                     : UserConsentTypes::NOT_GIVEN);
    play_consent.set_confirmation_grd_id(IDS_ARC_OPT_IN_DIALOG_BUTTON_AGREE);
    play_consent.set_consent_flow(
        UserConsentTypes::ArcPlayTermsOfServiceConsent::SETUP);
    if (tos_shown.value()) {
      play_consent.set_play_terms_of_service_text_length(tos_content->length());
      play_consent.set_play_terms_of_service_hash(
          base::SHA1HashString(*tos_content));
    }
    ConsentAuditorFactory::GetForProfile(profile_)->RecordArcPlayConsent(
        account_id, play_consent);

    // If the user - not policy - controls Backup and Restore setting, record
    // whether consent was given.
    if (!is_backup_restore_managed.value()) {
      UserConsentTypes::ArcBackupAndRestoreConsent backup_and_restore_consent;
      backup_and_restore_consent.set_confirmation_grd_id(
          IDS_ARC_OPT_IN_DIALOG_BUTTON_AGREE);
      backup_and_restore_consent.add_description_grd_ids(
          is_child ? IDS_ARC_OPT_IN_DIALOG_BACKUP_RESTORE_CHILD
                   : IDS_ARC_OPT_IN_DIALOG_BACKUP_RESTORE);
      backup_and_restore_consent.set_status(is_backup_restore_enabled.value()
                                                ? UserConsentTypes::GIVEN
                                                : UserConsentTypes::NOT_GIVEN);

      ConsentAuditorFactory::GetForProfile(profile_)
          ->RecordArcBackupAndRestoreConsent(account_id,
                                             backup_and_restore_consent);
    }

    // If the user - not policy - controls Location Services setting, record
    // whether consent was given.
    if (!is_location_service_managed.value()) {
      UserConsentTypes::ArcGoogleLocationServiceConsent
          location_service_consent;
      location_service_consent.set_confirmation_grd_id(
          IDS_ARC_OPT_IN_DIALOG_BUTTON_AGREE);

      if (ash::features::IsCrosPrivacyHubLocationEnabled()) {
        location_service_consent.add_description_grd_ids(
            is_child ? IDS_CROS_OPT_IN_LOCATION_SETTING_CHILD
                     : IDS_CROS_OPT_IN_LOCATION_SETTING);
      } else {
        location_service_consent.add_description_grd_ids(
            is_child ? IDS_ARC_OPT_IN_LOCATION_SETTING_CHILD
                     : IDS_ARC_OPT_IN_LOCATION_SETTING);
      }

      location_service_consent.set_status(is_location_service_enabled.value()
                                              ? UserConsentTypes::GIVEN
                                              : UserConsentTypes::NOT_GIVEN);
      ConsentAuditorFactory::GetForProfile(profile_)
          ->RecordArcGoogleLocationServiceConsent(account_id,
                                                  location_service_consent);
    }

    if (accepted) {
      // tos_delegate_->OnTermsAgreed() will free tos_delegate_. But will update
      // optin UI async to loading page. It is possible that user can click the
      // accept button again before optin UI is updated. b/284000632
      if (!tos_delegate_) {
        LOG(ERROR) << "tos_delegate_ has been freed.";
        return;
      }

      tos_delegate_->OnTermsAgreed(is_metrics_enabled.value(),
                                   is_backup_restore_enabled.value(),
                                   is_location_service_enabled.value());
    }
  } else if (*event == kEventOnRetryClicked) {
    // If ToS negotiation is ongoing, call the corresponding delegate.
    // Otherwise, call the general retry function.
    if (tos_delegate_) {
      tos_delegate_->OnTermsRetryClicked();
    } else {
      DCHECK(error_delegate_);
      error_delegate_->OnRetryClicked();
    }
  } else if (*event == kEventOnSendFeedbackClicked) {
    DCHECK(error_delegate_);
    error_delegate_->OnSendFeedbackClicked();
  } else if (*event == kEventOnRunNetworkTestsClicked) {
    DCHECK(error_delegate_);
    error_delegate_->OnRunNetworkTestsClicked();
  } else if (*event == kEventOnTosLoadResult) {
    if (tos_delegate_) {
      tos_delegate_->OnTermsLoadResult(
          message.FindBool(kSuccess).value_or(false));
    }
  } else if (*event == kEventOnErrorPageShown) {
    DCHECK(error_delegate_);
    error_delegate_->OnErrorPageShown(
        message.FindBool(kNetworkTestsShown).value_or(false));
  } else if (*event == kEventOnOpenPrivacySettingsPageClicked) {
    chrome::ShowSettingsSubPageForProfile(profile_, chrome::kPrivacySubPage);
  } else if (*event == kEventRequestWindowBounds) {
    SetWindowBound(display::Screen::GetScreen()->GetDisplayForNewWindows());
  } else {
    LOG(ERROR) << "Unknown message: " << *event;
    NOTREACHED_IN_MIGRATION();
  }
}
