// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_platform_bridge_win.h"

#include <memory>
#include <utility>

#include <objbase.h>
#include <wrl/event.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/win/core_winrt_util.h"
#include "base/win/registry.h"
#include "base/win/scoped_hstring.h"
#include "base/win/scoped_winrt_initializer.h"
#include "base/win/windows_version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_display_service_impl.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/notifications/win/notification_image_retainer.h"
#include "chrome/browser/notifications/win/notification_launch_id.h"
#include "chrome/browser/notifications/win/notification_metrics.h"
#include "chrome/browser/notifications/win/notification_template_builder.h"
#include "chrome/browser/notifications/win/notification_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/shell_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "ui/message_center/public/cpp/notification.h"

namespace mswr = Microsoft::WRL;
namespace winfoundtn = ABI::Windows::Foundation;
namespace winui = ABI::Windows::UI;
namespace winxml = ABI::Windows::Data::Xml;

using base::win::ScopedHString;
using message_center::RichNotificationData;
using notifications_uma::ActivationStatus;
using notifications_uma::CloseStatus;
using notifications_uma::DisplayStatus;
using notifications_uma::GetDisplayedLaunchIdStatus;
using notifications_uma::GetDisplayedStatus;
using notifications_uma::GetNotificationLaunchIdStatus;
using notifications_uma::GetSettingPolicy;
using notifications_uma::GetSettingStatus;
using notifications_uma::HandleEventStatus;
using notifications_uma::HistoryStatus;
using notifications_uma::OnDismissedStatus;
using notifications_uma::OnFailedStatus;
using notifications_uma::SetReadyCallbackStatus;

namespace {

// This string needs to be max 16 characters to work on Windows 10 prior to
// applying Creators Update (build 15063).
constexpr wchar_t kGroup[] = L"Notifications";

typedef winfoundtn::ITypedEventHandler<
    winui::Notifications::ToastNotification*,
    winui::Notifications::ToastDismissedEventArgs*>
    ToastDismissedHandler;

typedef winfoundtn::ITypedEventHandler<
    winui::Notifications::ToastNotification*,
    winui::Notifications::ToastFailedEventArgs*>
    ToastFailedHandler;

// Templated wrapper for winfoundtn::GetActivationFactory().
template <unsigned int size, typename T>
HRESULT CreateActivationFactory(wchar_t const (&class_name)[size], T** object) {
  ScopedHString ref_class_name =
      ScopedHString::Create(base::StringPiece16(class_name, size - 1));
  return base::win::RoGetActivationFactory(ref_class_name.get(),
                                           IID_PPV_ARGS(object));
}

void ForwardNotificationOperationOnUiThread(
    NotificationCommon::Operation operation,
    NotificationHandler::Type notification_type,
    const GURL& origin,
    const std::string& notification_id,
    const std::string& profile_id,
    bool incognito,
    const base::Optional<int>& action_index,
    const base::Optional<base::string16>& reply,
    const base::Optional<bool>& by_user) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!g_browser_process)
    return;

  // Profile ID can be empty for system notifications, which are not bound to a
  // profile, but system notifications are transient and thus not handled by
  // this NotificationPlatformBridge.
  // When transient notifications are supported, this should route the
  // notification response to the system NotificationDisplayService.
  DCHECK(!profile_id.empty());

  g_browser_process->profile_manager()->LoadProfile(
      profile_id, incognito,
      base::BindOnce(&NotificationDisplayServiceImpl::ProfileLoadedCallback,
                     operation, notification_type, origin, notification_id,
                     action_index, reply, by_user));
}

}  // namespace

// static
std::unique_ptr<NotificationPlatformBridge>
NotificationPlatformBridge::Create() {
  return std::make_unique<NotificationPlatformBridgeWin>();
}

// static
bool NotificationPlatformBridge::CanHandleType(
    NotificationHandler::Type notification_type) {
  return notification_type != NotificationHandler::Type::TRANSIENT;
}

class NotificationPlatformBridgeWinImpl
    : public base::RefCountedThreadSafe<NotificationPlatformBridgeWinImpl>,
      public content::BrowserThread::DeleteOnThread<
          content::BrowserThread::UI> {
 public:
  explicit NotificationPlatformBridgeWinImpl(
      scoped_refptr<base::SequencedTaskRunner> notification_task_runner)
      : com_functions_initialized_(
            base::win::ResolveCoreWinRTDelayload() &&
            ScopedHString::ResolveCoreWinRTStringDelayload()),
        notification_task_runner_(std::move(notification_task_runner)),
        image_retainer_(std::make_unique<NotificationImageRetainer>()) {
    // Delete any remaining temp files in the image folder from the previous
    // sessions.
    DCHECK(notification_task_runner_);
    content::BrowserThread::PostBestEffortTask(
        FROM_HERE, notification_task_runner_,
        image_retainer_->GetCleanupTask());
  }

  // Obtain an IToastNotification interface from a given XML as in
  // |xml_template|. This function is only used when displaying notification in
  // production code, which explains why the UMA metrics record within are
  // classified with the display path.
  mswr::ComPtr<winui::Notifications::IToastNotification> GetToastNotification(
      const message_center::Notification& notification,
      const base::string16& xml_template,
      const std::string& profile_id,
      bool incognito) {
    ScopedHString ref_class_name =
        ScopedHString::Create(RuntimeClass_Windows_Data_Xml_Dom_XmlDocument);
    mswr::ComPtr<IInspectable> inspectable;
    HRESULT hr =
        base::win::RoActivateInstance(ref_class_name.get(), &inspectable);
    if (FAILED(hr)) {
      LogDisplayHistogram(DisplayStatus::RO_ACTIVATE_FAILED);
      DLOG(ERROR) << "Unable to activate the XML Document " << std::hex << hr;
      return nullptr;
    }

    mswr::ComPtr<winxml::Dom::IXmlDocumentIO> document_io;
    hr = inspectable.As<winxml::Dom::IXmlDocumentIO>(&document_io);
    if (FAILED(hr)) {
      LogDisplayHistogram(
          DisplayStatus::CONVERSION_FAILED_INSPECTABLE_TO_XML_IO);
      DLOG(ERROR) << "Failed to get XmlDocument as IXmlDocumentIO " << std::hex
                  << hr;
      return nullptr;
    }

    ScopedHString ref_template = ScopedHString::Create(xml_template);
    hr = document_io->LoadXml(ref_template.get());
    if (FAILED(hr)) {
      LogDisplayHistogram(DisplayStatus::LOAD_XML_FAILED);
      DLOG(ERROR) << "Unable to load the template's XML into the document "
                  << std::hex << hr;
      return nullptr;
    }

    mswr::ComPtr<winxml::Dom::IXmlDocument> document;
    hr = document_io.As<winxml::Dom::IXmlDocument>(&document);
    if (FAILED(hr)) {
      LogDisplayHistogram(DisplayStatus::CONVERSION_FAILED_XML_IO_TO_XML);
      DLOG(ERROR) << "Unable to get as XMLDocument " << std::hex << hr;
      return nullptr;
    }

    mswr::ComPtr<winui::Notifications::IToastNotificationFactory>
        toast_notification_factory;
    hr = CreateActivationFactory(
        RuntimeClass_Windows_UI_Notifications_ToastNotification,
        toast_notification_factory.GetAddressOf());
    if (FAILED(hr)) {
      LogDisplayHistogram(DisplayStatus::CREATE_FACTORY_FAILED);
      DLOG(ERROR) << "Unable to create the IToastNotificationFactory "
                  << std::hex << hr;
      return nullptr;
    }

    mswr::ComPtr<winui::Notifications::IToastNotification> toast_notification;
    hr = toast_notification_factory->CreateToastNotification(
        document.Get(), toast_notification.GetAddressOf());
    if (FAILED(hr)) {
      LogDisplayHistogram(DisplayStatus::CREATE_TOAST_NOTIFICATION_FAILED);
      DLOG(ERROR) << "Unable to create the IToastNotification " << std::hex
                  << hr;
      return nullptr;
    }

    mswr::ComPtr<winui::Notifications::IToastNotification2> toast2;
    hr = toast_notification->QueryInterface(IID_PPV_ARGS(&toast2));
    if (FAILED(hr)) {
      LogDisplayHistogram(DisplayStatus::CREATE_TOAST_NOTIFICATION2_FAILED);
      DLOG(ERROR) << "Failed to get IToastNotification2 object " << std::hex
                  << hr;
      return nullptr;
    }

    // Set the Group and Tag values for the notification, in order to support
    // closing/replacing notification by tag. Both of these values have a limit
    // of 64 characters, which is problematic because they are out of our
    // control and Tag can contain just about anything. Therefore we use a hash
    // of the Tag value to produce uniqueness that fits within the specified
    // limits. Although Group is hard-coded, uniqueness is guaranteed through
    // features providing a sufficiently distinct notification id, profile id,
    // and incognito status combinations.
    ScopedHString group = ScopedHString::Create(kGroup);
    ScopedHString tag =
        ScopedHString::Create(GetTag(notification.id(), profile_id, incognito));

    hr = toast2->put_Group(group.get());
    if (FAILED(hr)) {
      LogDisplayHistogram(DisplayStatus::SETTING_GROUP_FAILED);
      DLOG(ERROR) << "Failed to set Group " << std::hex << hr;
      return nullptr;
    }

    hr = toast2->put_Tag(tag.get());
    if (FAILED(hr)) {
      LogDisplayHistogram(DisplayStatus::SETTING_TAG_FAILED);
      DLOG(ERROR) << "Failed to set Tag " << std::hex << hr;
      return nullptr;
    }

    // By default, Windows 10 will always show the notification on screen.
    // Chrome, however, wants to suppress them if both conditions are true:
    // 1) Renotify flag is not set.
    // 2) They are not new (no other notification with same tag is found).
    if (!notification.renotify()) {
      std::vector<mswr::ComPtr<winui::Notifications::IToastNotification>>
          notifications = GetNotifications(profile_id, incognito);

      for (const auto& notification : notifications) {
        mswr::ComPtr<winui::Notifications::IToastNotification2> t2;
        HRESULT hr = notification->QueryInterface(IID_PPV_ARGS(&t2));
        if (FAILED(hr))
          continue;

        HSTRING hstring_group;
        hr = t2->get_Group(&hstring_group);
        if (FAILED(hr)) {
          LogDisplayHistogram(DisplayStatus::GET_GROUP_FAILED);
          DLOG(ERROR) << "Failed to get group value " << std::hex << hr;
          return nullptr;
        }
        ScopedHString scoped_group(hstring_group);

        HSTRING hstring_tag;
        hr = t2->get_Tag(&hstring_tag);
        if (FAILED(hr)) {
          LogDisplayHistogram(DisplayStatus::GET_TAG_FAILED);
          DLOG(ERROR) << "Failed to get tag value " << std::hex << hr;
          return nullptr;
        }
        ScopedHString scoped_tag(hstring_tag);

        if (group.Get() != scoped_group.Get() || tag.Get() != scoped_tag.Get())
          continue;  // Because it is not a repeat of an toast.

        hr = toast2->put_SuppressPopup(true);
        if (FAILED(hr)) {
          LogDisplayHistogram(DisplayStatus::SUPPRESS_POPUP_FAILED);
          DLOG(ERROR) << "Failed to set suppress value " << std::hex << hr;
          return nullptr;
        }
      }
    }

    return toast_notification;
  }

  void Display(NotificationHandler::Type notification_type,
               const std::string& profile_id,
               bool incognito,
               std::unique_ptr<message_center::Notification> notification,
               std::unique_ptr<NotificationCommon::Metadata> metadata) {
    // TODO(finnur): Move this to a RoInitialized thread, as per
    // crbug.com/761039.
    DCHECK(notification_task_runner_->RunsTasksInCurrentSequence());

    if (!notifier_for_testing_ && !notifier_.Get() &&
        FAILED(InitializeToastNotifier())) {
      // A histogram should have already been logged for this failure.
      DLOG(ERROR) << "Unable to initialize toast notifier";
      return;
    }

    winui::Notifications::IToastNotifier* notifier =
        notifier_for_testing_ ? notifier_for_testing_ : notifier_.Get();

    winui::Notifications::NotificationSetting setting;
    HRESULT hr = notifier->get_Setting(&setting);
    if (SUCCEEDED(hr)) {
      LogGetSettingStatus(GetSettingStatus::SUCCESS);
      switch (setting) {
        case winui::Notifications::NotificationSetting_Enabled:
          LogGetSettingPolicy(GetSettingPolicy::ENABLED);
          break;
        case winui::Notifications::NotificationSetting_DisabledForApplication:
          LogGetSettingPolicy(GetSettingPolicy::DISABLED_FOR_APPLICATION);
          DLOG(ERROR) << "Notifications disabled for application";
          break;
        case winui::Notifications::NotificationSetting_DisabledForUser:
          LogGetSettingPolicy(GetSettingPolicy::DISABLED_FOR_USER);
          DLOG(ERROR) << "Notifications disabled for user";
          break;
        case winui::Notifications::NotificationSetting_DisabledByGroupPolicy:
          LogGetSettingPolicy(GetSettingPolicy::DISABLED_BY_GROUP_POLICY);
          DLOG(ERROR) << "Notifications disabled by group policy";
          break;
        case winui::Notifications::NotificationSetting_DisabledByManifest:
          LogGetSettingPolicy(GetSettingPolicy::DISABLED_BY_MANIFEST);
          DLOG(ERROR) << "Notifications disabled by manifest";
          break;
      }
    } else {
      LogGetSettingStatus(GetSettingStatus::UNKNOWN_FAILURE);
    }

    NotificationLaunchId launch_id(notification_type, notification->id(),
                                   profile_id, incognito,
                                   notification->origin_url());
    base::string16 xml_template = BuildNotificationTemplate(
        image_retainer_.get(), launch_id, *notification);
    mswr::ComPtr<winui::Notifications::IToastNotification> toast =
        GetToastNotification(*notification, xml_template, profile_id,
                             incognito);
    if (!toast)
      return;

    // Activation via user interaction with the toast is handled in
    // HandleActivation() by way of the notification_helper.

    auto dismissed_handler = mswr::Callback<ToastDismissedHandler>(
        this, &NotificationPlatformBridgeWinImpl::OnDismissed);
    EventRegistrationToken dismissed_token;
    hr = toast->add_Dismissed(dismissed_handler.Get(), &dismissed_token);
    if (FAILED(hr)) {
      LogDisplayHistogram(DisplayStatus::ADD_TOAST_DISMISS_HANDLER_FAILED);
      DLOG(ERROR) << "Unable to add toast dismissed event handler " << std::hex
                  << hr;
      return;
    }

    auto failed_handler = mswr::Callback<ToastFailedHandler>(
        this, &NotificationPlatformBridgeWinImpl::OnFailed);
    EventRegistrationToken failed_token;
    hr = toast->add_Failed(failed_handler.Get(), &failed_token);
    if (FAILED(hr)) {
      LogDisplayHistogram(DisplayStatus::ADD_TOAST_ERROR_HANDLER_FAILED);
      DLOG(ERROR) << "Unable to add toast failed event handler " << std::hex
                  << hr;
      return;
    }

    hr = notifier->Show(toast.Get());
    if (FAILED(hr)) {
      LogDisplayHistogram(DisplayStatus::SHOWING_TOAST_FAILED);
      base::UmaHistogramSparse("Notifications.Windows.ShowFailedErrorCode", hr);
      DLOG(ERROR) << "Unable to display the notification " << std::hex << hr;
    } else {
      LogDisplayHistogram(DisplayStatus::SUCCESS);
    }
  }

  void Close(const std::string& profile_id,
             bool incognito,
             const std::string& notification_id) {
    DCHECK(notification_task_runner_->RunsTasksInCurrentSequence());

    mswr::ComPtr<winui::Notifications::IToastNotificationHistory> history =
        GetIToastNotificationHistory();
    if (!history) {
      LogCloseHistogram(CloseStatus::GET_TOAST_HISTORY_FAILED);
      DLOG(ERROR) << "Failed to get IToastNotificationHistory";
      return;
    }

    ScopedHString application_id = ScopedHString::Create(GetAppId());
    ScopedHString group = ScopedHString::Create(kGroup);
    ScopedHString tag =
        ScopedHString::Create(GetTag(notification_id, profile_id, incognito));

    HRESULT hr = history->RemoveGroupedTagWithId(tag.get(), group.get(),
                                                 application_id.get());
    if (FAILED(hr)) {
      LogCloseHistogram(CloseStatus::REMOVING_TOAST_FAILED);
      DLOG(ERROR) << "Failed to remove notification with id "
                  << notification_id.c_str() << " " << std::hex << hr;
    } else {
      LogCloseHistogram(CloseStatus::SUCCESS);
    }
  }

  mswr::ComPtr<winui::Notifications::IToastNotificationHistory>
  GetIToastNotificationHistory() const WARN_UNUSED_RESULT {
    mswr::ComPtr<winui::Notifications::IToastNotificationManagerStatics>
        toast_manager;
    HRESULT hr = CreateActivationFactory(
        RuntimeClass_Windows_UI_Notifications_ToastNotificationManager,
        toast_manager.GetAddressOf());
    if (FAILED(hr)) {
      LogHistoryHistogram(
          HistoryStatus::CREATE_TOAST_NOTIFICATION_MANAGER_FAILED);
      DLOG(ERROR) << "Unable to create the ToastNotificationManager "
                  << std::hex << hr;
      return nullptr;
    }

    mswr::ComPtr<winui::Notifications::IToastNotificationManagerStatics2>
        toast_manager2;
    hr = toast_manager
             .As<winui::Notifications::IToastNotificationManagerStatics2>(
                 &toast_manager2);
    if (FAILED(hr)) {
      LogHistoryHistogram(
          HistoryStatus::QUERY_TOAST_MANAGER_STATISTICS2_FAILED);
      DLOG(ERROR) << "Failed to get IToastNotificationManagerStatics2 "
                  << std::hex << hr;
      return nullptr;
    }

    mswr::ComPtr<winui::Notifications::IToastNotificationHistory>
        notification_history;
    hr = toast_manager2->get_History(notification_history.GetAddressOf());
    if (FAILED(hr)) {
      LogHistoryHistogram(HistoryStatus::GET_TOAST_HISTORY_FAILED);
      DLOG(ERROR) << "Failed to get IToastNotificationHistory " << std::hex
                  << hr;
      return nullptr;
    }

    LogHistoryHistogram(HistoryStatus::SUCCESS);

    return notification_history;
  }

  std::vector<mswr::ComPtr<winui::Notifications::IToastNotification>>
  GetDisplayedFromActionCenter(const std::string& profile_id,
                               bool incognito) const {
    mswr::ComPtr<winui::Notifications::IToastNotificationHistory> history =
        GetIToastNotificationHistory();
    if (!history) {
      LogGetDisplayedStatus(GetDisplayedStatus::GET_TOAST_HISTORY_FAILED);
      DLOG(ERROR) << "Failed to get IToastNotificationHistory";
      return {};
    }

    mswr::ComPtr<winui::Notifications::IToastNotificationHistory2> history2;
    HRESULT hr =
        history.As<winui::Notifications::IToastNotificationHistory2>(&history2);
    if (FAILED(hr)) {
      LogGetDisplayedStatus(
          GetDisplayedStatus::QUERY_TOAST_NOTIFICATION_HISTORY2_FAILED);
      DLOG(ERROR) << "Failed to get IToastNotificationHistory2 " << std::hex
                  << hr;
      return {};
    }

    ScopedHString application_id = ScopedHString::Create(GetAppId());

    mswr::ComPtr<winfoundtn::Collections::IVectorView<
        winui::Notifications::ToastNotification*>>
        list;
    hr = history2->GetHistoryWithId(application_id.get(), &list);
    if (FAILED(hr)) {
      LogGetDisplayedStatus(GetDisplayedStatus::GET_HISTORY_WITH_ID_FAILED);
      DLOG(ERROR) << "GetHistoryWithId failed " << std::hex << hr;
      return {};
    }

    uint32_t size;
    hr = list->get_Size(&size);
    if (FAILED(hr)) {
      LogGetDisplayedStatus(GetDisplayedStatus::GET_SIZE_FAILED);
      DLOG(ERROR) << "History get_Size call failed " << std::hex << hr;
      return {};
    }

    GetDisplayedStatus status = GetDisplayedStatus::SUCCESS;

    std::vector<mswr::ComPtr<winui::Notifications::IToastNotification>>
        notifications;
    for (uint32_t index = 0; index < size; ++index) {
      mswr::ComPtr<winui::Notifications::IToastNotification> tn;
      hr = list->GetAt(index, &tn);
      if (FAILED(hr)) {
        status = GetDisplayedStatus::SUCCESS_WITH_GET_AT_FAILURE;
        DLOG(ERROR) << "Failed to get notification " << index << " of " << size
                    << " " << std::hex << hr;
        continue;
      }
      notifications.push_back(std::move(tn));
    }

    LogGetDisplayedStatus(status);
    return notifications;
  }

  std::vector<mswr::ComPtr<winui::Notifications::IToastNotification>>
  GetNotifications(const std::string& profile_id, bool incognito) const {
    if (NotificationPlatformBridgeWinImpl::notifications_for_testing_)
      return *NotificationPlatformBridgeWinImpl::notifications_for_testing_;

    return GetDisplayedFromActionCenter(profile_id, incognito);
  }

  void GetDisplayed(const std::string& profile_id,
                    bool incognito,
                    GetDisplayedNotificationsCallback callback) const {
    DCHECK(notification_task_runner_->RunsTasksInCurrentSequence());

    std::vector<mswr::ComPtr<winui::Notifications::IToastNotification>>
        notifications = GetNotifications(profile_id, incognito);

    std::set<std::string> displayed_notifications;
    for (const auto& notification : notifications) {
      NotificationLaunchId launch_id(
          GetNotificationLaunchId(notification.Get()));
      if (!launch_id.is_valid()) {
        LogGetDisplayedLaunchIdStatus(
            GetDisplayedLaunchIdStatus::DECODE_LAUNCH_ID_FAILED);
        DLOG(ERROR) << "Failed to decode notification ID";
        continue;
      }
      if (launch_id.profile_id() != profile_id ||
          launch_id.incognito() != incognito) {
        continue;
      }
      LogGetDisplayedLaunchIdStatus(GetDisplayedLaunchIdStatus::SUCCESS);
      displayed_notifications.insert(launch_id.notification_id());
    }

    base::PostTask(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(std::move(callback), std::move(displayed_notifications),
                       /*supports_synchronization=*/true));
  }

  // Test to see if the notification_helper.exe has been registered in the
  // system, either under HKCU or HKLM.
  bool IsToastActivatorRegistered() {
    base::win::RegKey key;
    base::string16 path =
        InstallUtil::GetToastActivatorRegistryPath() + L"\\LocalServer32";
    HKEY root = install_static::IsSystemInstall() ? HKEY_LOCAL_MACHINE
                                                  : HKEY_CURRENT_USER;
    return ERROR_SUCCESS == key.Open(root, path.c_str(), KEY_QUERY_VALUE);
  }

  void SetReadyCallback(
      NotificationPlatformBridge::NotificationBridgeReadyCallback callback) {
    DCHECK(notification_task_runner_->RunsTasksInCurrentSequence());

    bool activator_registered = IsToastActivatorRegistered();
    bool shortcut_installed =
        InstallUtil::IsStartMenuShortcutWithActivatorGuidInstalled();

    int status = static_cast<int>(SetReadyCallbackStatus::SUCCESS);
    bool enabled = com_functions_initialized_ && activator_registered &&
                   shortcut_installed;

    if (!enabled) {
      if (!shortcut_installed) {
        status |=
            static_cast<int>(SetReadyCallbackStatus::SHORTCUT_MISCONFIGURATION);
      }
      if (!activator_registered) {
        status |= static_cast<int>(
            SetReadyCallbackStatus::COM_SERVER_MISCONFIGURATION);
      }
      if (!com_functions_initialized_)
        status |= static_cast<int>(SetReadyCallbackStatus::COM_NOT_INITIALIZED);
    }

    LogSetReadyCallbackStatus(static_cast<SetReadyCallbackStatus>(status));

    bool success = base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                                  base::BindOnce(std::move(callback), enabled));
    DCHECK(success);
  }

  void HandleEvent(winui::Notifications::IToastNotification* notification,
                   NotificationCommon::Operation operation,
                   const base::Optional<int>& action_index,
                   const base::Optional<bool>& by_user) {
    NotificationLaunchId launch_id(GetNotificationLaunchId(notification));
    if (!launch_id.is_valid()) {
      LogHandleEventStatus(HandleEventStatus::HANDLE_EVENT_LAUNCH_ID_INVALID);
      DLOG(ERROR) << "Failed to decode launch ID for operation " << operation;
      return;
    }

    base::PostTask(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(&ForwardNotificationOperationOnUiThread, operation,
                       launch_id.notification_type(), launch_id.origin_url(),
                       launch_id.notification_id(), launch_id.profile_id(),
                       launch_id.incognito(), action_index,
                       /*reply=*/base::nullopt, by_user));
    LogHandleEventStatus(HandleEventStatus::SUCCESS);
  }

  base::Optional<int> ParseActionIndex(
      winui::Notifications::IToastActivatedEventArgs* args) {
    HSTRING arguments;
    HRESULT hr = args->get_Arguments(&arguments);
    if (FAILED(hr))
      return base::nullopt;

    ScopedHString arguments_scoped(arguments);
    NotificationLaunchId launch_id(arguments_scoped.GetAsUTF8());
    return (launch_id.is_valid() && launch_id.button_index() >= 0)
               ? base::Optional<int>(launch_id.button_index())
               : base::nullopt;
  }

  void ForwardHandleEventForTesting(
      NotificationCommon::Operation operation,
      winui::Notifications::IToastNotification* notification,
      winui::Notifications::IToastActivatedEventArgs* args,
      const base::Optional<bool>& by_user) {
    base::Optional<int> action_index = ParseActionIndex(args);
    HandleEvent(notification, operation, action_index, by_user);
  }

 private:
  friend class base::RefCountedThreadSafe<NotificationPlatformBridgeWinImpl>;
  friend class NotificationPlatformBridgeWin;

  ~NotificationPlatformBridgeWinImpl() {
    notification_task_runner_->DeleteSoon(FROM_HERE, image_retainer_.release());
  }

  base::string16 GetAppId() const {
    return ShellUtil::GetBrowserModelId(InstallUtil::IsPerUserInstall());
  }

  base::string16 GetTag(const std::string& notification_id,
                        const std::string& profile_id,
                        bool incognito) {
    std::string payload = base::StringPrintf(
        "%s|%s|%d", notification_id.c_str(), profile_id.c_str(), incognito);
    return base::NumberToString16(base::Hash(payload));
  }

  HRESULT OnDismissed(
      winui::Notifications::IToastNotification* notification,
      winui::Notifications::IToastDismissedEventArgs* arguments) {
    base::Optional<bool> by_user = base::nullopt;

    winui::Notifications::ToastDismissalReason reason;
    HRESULT hr = arguments->get_Reason(&reason);
    if (SUCCEEDED(hr)) {
      LogOnDismissedStatus(OnDismissedStatus::SUCCESS);
      by_user = base::Optional<bool>(
          reason == winui::Notifications::ToastDismissalReason_UserCanceled);
    } else {
      LogOnDismissedStatus(OnDismissedStatus::GET_DISMISSAL_REASON_FAILED);
      DLOG(ERROR) << "Failed to get toast dismissal reason: " << std::hex << hr;
    }

    HandleEvent(notification, NotificationCommon::OPERATION_CLOSE,
                /*action_index=*/base::nullopt, by_user);
    return S_OK;
  }

  HRESULT OnFailed(winui::Notifications::IToastNotification* notification,
                   winui::Notifications::IToastFailedEventArgs* arguments) {
    HRESULT error_code;
    HRESULT hr = arguments->get_ErrorCode(&error_code);
    if (SUCCEEDED(hr)) {
      // Error code successfully obtained from the Action Center.
      LogOnFailedStatus(OnFailedStatus::SUCCESS);
      base::UmaHistogramSparse("Notifications.Windows.DisplayFailure",
                               error_code);
      DLOG(ERROR) << "Failed to raise the toast notification, error code: "
                  << std::hex << error_code;
    } else {
      LogOnFailedStatus(OnFailedStatus::GET_ERROR_CODE_FAILED);
      DLOG(ERROR) << "Failed to raise the toast notification; failed to get "
                     "error code: "
                  << std::hex << hr;
    }

    return S_OK;
  }

  HRESULT InitializeToastNotifier() {
    mswr::ComPtr<winui::Notifications::IToastNotificationManagerStatics>
        toast_manager;
    HRESULT hr = CreateActivationFactory(
        RuntimeClass_Windows_UI_Notifications_ToastNotificationManager,
        toast_manager.GetAddressOf());
    if (FAILED(hr)) {
      LogDisplayHistogram(
          DisplayStatus::CREATE_TOAST_NOTIFICATION_MANAGER_FAILED);
      DLOG(ERROR) << "Unable to create the ToastNotificationManager "
                  << std::hex << hr;
      return hr;
    }

    ScopedHString application_id = ScopedHString::Create(GetAppId());
    hr = toast_manager->CreateToastNotifierWithId(application_id.get(),
                                                  &notifier_);
    if (FAILED(hr)) {
      LogDisplayHistogram(DisplayStatus::CREATE_TOAST_NOTIFIER_WITH_ID_FAILED);
      base::UmaHistogramSparse(
          "Notifications.Windows.CreateToastManagerErrorCode", hr);
      DLOG(ERROR) << "Unable to create the ToastNotifier " << std::hex << hr;
    }
    return hr;
  }

  static std::vector<mswr::ComPtr<winui::Notifications::IToastNotification>>*
      notifications_for_testing_;

  static winui::Notifications::IToastNotifier* notifier_for_testing_;

  // Whether the required functions from combase.dll have been loaded.
  const bool com_functions_initialized_;

  // The task runner running notification related tasks.
  scoped_refptr<base::SequencedTaskRunner> notification_task_runner_;

  // An object that keeps temp files alive long enough for Windows to pick up.
  std::unique_ptr<NotificationImageRetainer> image_retainer_;

  // The ToastNotifier to use to communicate with the Action Center.
  mswr::ComPtr<winui::Notifications::IToastNotifier> notifier_;

  DISALLOW_COPY_AND_ASSIGN(NotificationPlatformBridgeWinImpl);
};

std::vector<mswr::ComPtr<winui::Notifications::IToastNotification>>*
    NotificationPlatformBridgeWinImpl::notifications_for_testing_ = nullptr;

winui::Notifications::IToastNotifier*
    NotificationPlatformBridgeWinImpl::notifier_for_testing_ = nullptr;

NotificationPlatformBridgeWin::NotificationPlatformBridgeWin() {
  notification_task_runner_ = base::CreateSequencedTaskRunner(
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  impl_ = base::MakeRefCounted<NotificationPlatformBridgeWinImpl>(
      notification_task_runner_);
}

NotificationPlatformBridgeWin::~NotificationPlatformBridgeWin() = default;

void NotificationPlatformBridgeWin::Display(
    NotificationHandler::Type notification_type,
    Profile* profile,
    const message_center::Notification& notification,
    std::unique_ptr<NotificationCommon::Metadata> metadata) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Make a deep copy of the notification as its resources cannot safely
  // be passed between threads.
  auto notification_copy = message_center::Notification::DeepCopy(
      notification, /*include_body_image=*/true, /*include_small_image=*/true,
      /*include_icon_images=*/true);

  notification_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&NotificationPlatformBridgeWinImpl::Display, impl_,
                     notification_type, GetProfileId(profile),
                     profile->IsOffTheRecord(), std::move(notification_copy),
                     std::move(metadata)));
}

void NotificationPlatformBridgeWin::Close(Profile* profile,
                                          const std::string& notification_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  notification_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&NotificationPlatformBridgeWinImpl::Close,
                                impl_, GetProfileId(profile),
                                profile->IsOffTheRecord(), notification_id));
}

void NotificationPlatformBridgeWin::GetDisplayed(
    Profile* profile,
    GetDisplayedNotificationsCallback callback) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  notification_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&NotificationPlatformBridgeWinImpl::GetDisplayed, impl_,
                     GetProfileId(profile), profile->IsOffTheRecord(),
                     std::move(callback)));
}

void NotificationPlatformBridgeWin::SetReadyCallback(
    NotificationBridgeReadyCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  notification_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&NotificationPlatformBridgeWinImpl::SetReadyCallback,
                     impl_, std::move(callback)));
}

void NotificationPlatformBridgeWin::DisplayServiceShutDown(Profile* profile) {}

// static
bool NotificationPlatformBridgeWin::HandleActivation(
    const base::CommandLine& command_line) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  NotificationLaunchId launch_id(base::UTF16ToUTF8(
      command_line.GetSwitchValueNative(switches::kNotificationLaunchId)));
  if (!launch_id.is_valid()) {
    LogActivationStatus(ActivationStatus::INVALID_LAUNCH_ID);
    return false;
  }

  base::Optional<base::string16> reply;
  base::string16 inline_reply =
      command_line.GetSwitchValueNative(switches::kNotificationInlineReply);
  if (!inline_reply.empty())
    reply = inline_reply;

  NotificationCommon::Operation operation;
  if (launch_id.is_for_dismiss_button())
    operation = NotificationCommon::OPERATION_CLOSE;
  else if (launch_id.is_for_context_menu())
    operation = NotificationCommon::OPERATION_SETTINGS;
  else
    operation = NotificationCommon::OPERATION_CLICK;

  base::Optional<int> action_index;
  if (launch_id.button_index() != -1)
    action_index = launch_id.button_index();

  ForwardNotificationOperationOnUiThread(
      operation, launch_id.notification_type(), launch_id.origin_url(),
      launch_id.notification_id(), launch_id.profile_id(),
      launch_id.incognito(), std::move(action_index), reply, /*by_user=*/true);

  LogActivationStatus(ActivationStatus::SUCCESS);
  return true;
}

// static
bool NotificationPlatformBridgeWin::NativeNotificationEnabled() {
  // There was a Microsoft bug in Windows 10 prior to build 17134 (i.e.,
  // Version::WIN10_RS4), causing endless loops in displaying
  // notifications. It significantly amplified the memory and CPU usage.
  // Therefore, we enable Windows 10 native notification only for build 17134
  // and later. See crbug.com/882622 and crbug.com/878823 for more details.
  return base::win::GetVersion() >= base::win::Version::WIN10_RS4 &&
         base::FeatureList::IsEnabled(features::kNativeNotifications);
}

void NotificationPlatformBridgeWin::ForwardHandleEventForTesting(
    NotificationCommon::Operation operation,
    winui::Notifications::IToastNotification* notification,
    winui::Notifications::IToastActivatedEventArgs* args,
    const base::Optional<bool>& by_user) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  notification_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &NotificationPlatformBridgeWinImpl::ForwardHandleEventForTesting,
          impl_, operation, base::Unretained(notification),
          base::Unretained(args), by_user));
}

void NotificationPlatformBridgeWin::SetDisplayedNotificationsForTesting(
    std::vector<mswr::ComPtr<winui::Notifications::IToastNotification>>*
        notifications) {
  NotificationPlatformBridgeWinImpl::notifications_for_testing_ = notifications;
}

void NotificationPlatformBridgeWin::SetNotifierForTesting(
    winui::Notifications::IToastNotifier* notifier) {
  NotificationPlatformBridgeWinImpl::notifier_for_testing_ = notifier;
}

mswr::ComPtr<winui::Notifications::IToastNotification>
NotificationPlatformBridgeWin::GetToastNotificationForTesting(
    const message_center::Notification& notification,
    const base::string16& xml_template,
    const std::string& profile_id,
    bool incognito) {
  return impl_->GetToastNotification(notification, xml_template, profile_id,
                                     incognito);
}
