// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_platform_bridge_win.h"

#include <memory>
#include <utility>

#include <objbase.h>
#include <wrl/event.h>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/win/core_winrt_util.h"
#include "base/win/registry.h"
#include "base/win/scoped_hstring.h"
#include "base/win/scoped_winrt_initializer.h"
#include "base/win/windows_version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_display_service_impl.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/notifications/win/notification_metrics.h"
#include "chrome/browser/notifications/win/notification_template_builder.h"
#include "chrome/browser/notifications/win/notification_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notifications/notification_image_retainer.h"
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
template <unsigned int size>
HRESULT CreateActivationFactory(wchar_t const (&class_name)[size],
                                const IID& iid,
                                void** factory) {
  ScopedHString ref_class_name =
      ScopedHString::Create(base::WStringPiece(class_name, size - 1));
  return base::win::RoGetActivationFactory(ref_class_name.get(), iid, factory);
}

void ForwardNotificationOperationOnUiThread(
    NotificationOperation operation,
    NotificationHandler::Type notification_type,
    const GURL& origin,
    const std::string& notification_id,
    const std::string& profile_id,
    bool incognito,
    const absl::optional<int>& action_index,
    const absl::optional<std::u16string>& reply,
    const absl::optional<bool>& by_user) {
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
      NotificationPlatformBridge::GetProfileBaseNameFromProfileId(profile_id),
      incognito,
      base::BindOnce(&NotificationDisplayServiceImpl::ProfileLoadedCallback,
                     operation, notification_type, origin, notification_id,
                     action_index, reply, by_user));
}

GetSettingPolicy ConvertSettingPolicy(
    winui::Notifications::NotificationSetting setting) {
  switch (setting) {
    case winui::Notifications::NotificationSetting_Enabled:
      return GetSettingPolicy::kEnabled;
    case winui::Notifications::NotificationSetting_DisabledForApplication:
      DLOG(ERROR) << "Notifications disabled for application";
      return GetSettingPolicy::kDisabledForApplication;
    case winui::Notifications::NotificationSetting_DisabledForUser:
      DLOG(ERROR) << "Notifications disabled for user";
      return GetSettingPolicy::kDisabledForUser;
    case winui::Notifications::NotificationSetting_DisabledByGroupPolicy:
      DLOG(ERROR) << "Notifications disabled by group policy";
      return GetSettingPolicy::kDisabledByGroupPolicy;
    case winui::Notifications::NotificationSetting_DisabledByManifest:
      DLOG(ERROR) << "Notifications disabled by manifest";
      return GetSettingPolicy::kDisabledByManifest;
  }
  DLOG(ERROR) << "Unknown Windows notification setting";
  return GetSettingPolicy::kUnknown;
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
      : com_functions_initialized_(base::win::ResolveCoreWinRTDelayload()),
        notification_task_runner_(std::move(notification_task_runner)),
        image_retainer_(std::make_unique<NotificationImageRetainer>()) {
    // Delete any remaining temp files in the image folder from the previous
    // sessions.
    DCHECK(notification_task_runner_);
    content::BrowserThread::PostBestEffortTask(
        FROM_HERE, notification_task_runner_,
        image_retainer_->GetCleanupTask());

    notification_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &NotificationPlatformBridgeWinImpl::InitializeOnTaskRunner,
            base::Unretained(this)));
  }
  NotificationPlatformBridgeWinImpl(const NotificationPlatformBridgeWinImpl&) =
      delete;
  NotificationPlatformBridgeWinImpl& operator=(
      const NotificationPlatformBridgeWinImpl&) = delete;

  // Obtain an IToastNotification interface from a given XML as in
  // |xml_template|. This function is only used when displaying notification in
  // production code, which explains why the UMA metrics record within are
  // classified with the display path.
  mswr::ComPtr<winui::Notifications::IToastNotification> GetToastNotification(
      const message_center::Notification& notification,
      const std::wstring& xml_template,
      const std::string& profile_id,
      bool incognito) {
    ScopedHString ref_class_name =
        ScopedHString::Create(RuntimeClass_Windows_Data_Xml_Dom_XmlDocument);
    mswr::ComPtr<IInspectable> inspectable;
    HRESULT hr =
        base::win::RoActivateInstance(ref_class_name.get(), &inspectable);
    if (FAILED(hr)) {
      LogDisplayHistogram(DisplayStatus::kRoActivateFailed);
      DLOG(ERROR) << "Unable to activate the XML Document " << std::hex << hr;
      return nullptr;
    }

    mswr::ComPtr<winxml::Dom::IXmlDocumentIO> document_io;
    hr = inspectable.As(&document_io);
    if (FAILED(hr)) {
      LogDisplayHistogram(DisplayStatus::kConversionFailedInspectableToXmlIo);
      DLOG(ERROR) << "Failed to get XmlDocument as IXmlDocumentIO " << std::hex
                  << hr;
      return nullptr;
    }

    ScopedHString ref_template = ScopedHString::Create(xml_template);
    hr = document_io->LoadXml(ref_template.get());
    if (FAILED(hr)) {
      LogDisplayHistogram(DisplayStatus::kLoadXmlFailed);
      DLOG(ERROR) << "Unable to load the template's XML into the document "
                  << std::hex << hr;
      return nullptr;
    }

    mswr::ComPtr<winxml::Dom::IXmlDocument> document;
    hr = document_io.As<winxml::Dom::IXmlDocument>(&document);
    if (FAILED(hr)) {
      LogDisplayHistogram(DisplayStatus::kConversionFailedXmlIoToXml);
      DLOG(ERROR) << "Unable to get as XMLDocument " << std::hex << hr;
      return nullptr;
    }

    mswr::ComPtr<winui::Notifications::IToastNotificationFactory>
        toast_notification_factory;
    hr = CreateActivationFactory(
        RuntimeClass_Windows_UI_Notifications_ToastNotification,
        IID_PPV_ARGS(&toast_notification_factory));
    if (FAILED(hr)) {
      LogDisplayHistogram(DisplayStatus::kCreateFactoryFailed);
      DLOG(ERROR) << "Unable to create the IToastNotificationFactory "
                  << std::hex << hr;
      return nullptr;
    }

    mswr::ComPtr<winui::Notifications::IToastNotification> toast_notification;
    hr = toast_notification_factory->CreateToastNotification(
        document.Get(), &toast_notification);
    if (FAILED(hr)) {
      LogDisplayHistogram(DisplayStatus::kCreateToastNotificationFailed);
      DLOG(ERROR) << "Unable to create the IToastNotification " << std::hex
                  << hr;
      return nullptr;
    }

    mswr::ComPtr<winui::Notifications::IToastNotification2> toast2;
    hr = toast_notification->QueryInterface(IID_PPV_ARGS(&toast2));
    if (FAILED(hr)) {
      LogDisplayHistogram(DisplayStatus::kCreateToastNotification2Failed);
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
      LogDisplayHistogram(DisplayStatus::kSettingGroupFailed);
      DLOG(ERROR) << "Failed to set Group " << std::hex << hr;
      return nullptr;
    }

    hr = toast2->put_Tag(tag.get());
    if (FAILED(hr)) {
      LogDisplayHistogram(DisplayStatus::kSettingTagFailed);
      DLOG(ERROR) << "Failed to set Tag " << std::hex << hr;
      return nullptr;
    }

    // By default, Windows 10 will always show the notification on screen.
    // Chrome, however, wants to suppress them if both conditions are true:
    // 1) Renotify flag is not set.
    // 2) They are not new (no other notification with same tag is found).
    if (!notification.renotify()) {
      std::vector<mswr::ComPtr<winui::Notifications::IToastNotification>>
          notifications = GetNotifications();

      for (const auto& n : notifications) {
        mswr::ComPtr<winui::Notifications::IToastNotification2> t2;
        hr = n->QueryInterface(IID_PPV_ARGS(&t2));
        if (FAILED(hr))
          continue;

        HSTRING hstring_group;
        hr = t2->get_Group(&hstring_group);
        if (FAILED(hr)) {
          LogDisplayHistogram(DisplayStatus::kGetGroupFailed);
          DLOG(ERROR) << "Failed to get group value " << std::hex << hr;
          return nullptr;
        }
        ScopedHString scoped_group(hstring_group);

        HSTRING hstring_tag;
        hr = t2->get_Tag(&hstring_tag);
        if (FAILED(hr)) {
          LogDisplayHistogram(DisplayStatus::kGetTagFailed);
          DLOG(ERROR) << "Failed to get tag value " << std::hex << hr;
          return nullptr;
        }
        ScopedHString scoped_tag(hstring_tag);

        if (group.Get() != scoped_group.Get() || tag.Get() != scoped_tag.Get())
          continue;  // Because it is not a repeat of an toast.

        hr = toast2->put_SuppressPopup(true);
        if (FAILED(hr)) {
          LogDisplayHistogram(DisplayStatus::kSuppressPopupFailed);
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
      LogGetSettingStatus(GetSettingStatus::kSuccess);
      LogGetSettingPolicy(ConvertSettingPolicy(setting));
    } else {
      LogGetSettingStatus(GetSettingStatus::kUnknownFailure);
    }

    NotificationLaunchId launch_id(notification_type, notification->id(),
                                   profile_id, incognito,
                                   notification->origin_url());
    std::wstring xml_template = BuildNotificationTemplate(
        image_retainer_.get(), launch_id, *notification);
    mswr::ComPtr<winui::Notifications::IToastNotification> toast =
        GetToastNotification(*notification, xml_template, profile_id,
                             incognito);
    if (!toast)
      return;

    // Activation via user interaction with the toast is handled in
    // HandleActivation() by way of the notification_helper.

    auto failed_handler = mswr::Callback<ToastFailedHandler>(
        this, &NotificationPlatformBridgeWinImpl::OnFailed);
    EventRegistrationToken failed_token;
    hr = toast->add_Failed(failed_handler.Get(), &failed_token);
    if (FAILED(hr)) {
      LogDisplayHistogram(DisplayStatus::kAddToastErrorHandlerFailed);
      DLOG(ERROR) << "Unable to add toast failed event handler " << std::hex
                  << hr;
      return;
    }

    hr = notifier->Show(toast.Get());
    if (FAILED(hr)) {
      LogDisplayHistogram(DisplayStatus::kShowingToastFailed);
      base::UmaHistogramSparse("Notifications.Windows.ShowFailedErrorCode", hr);
      DLOG(ERROR) << "Unable to display the notification " << std::hex << hr;
    } else {
      // Store locally the notification launch id and use it to synchronize
      // browser notifications with the displayed notifications to simulate
      // close events.
      displayed_notifications_[{profile_id, notification->id()}] =
          GetNotificationLaunchId(toast.Get());

      LogDisplayHistogram(DisplayStatus::kSuccess);

      MaybeStartNotificationSynchronizationTimer();
    }
  }

  void Close(const std::string& profile_id,
             bool incognito,
             const std::string& notification_id) {
    DCHECK(notification_task_runner_->RunsTasksInCurrentSequence());

    mswr::ComPtr<winui::Notifications::IToastNotificationHistory> history =
        GetIToastNotificationHistory();
    if (!history) {
      LogCloseHistogram(CloseStatus::kGetToastHistoryFailed);
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
      LogCloseHistogram(CloseStatus::kRemovingToastFailed);
      DLOG(ERROR) << "Failed to remove notification with id "
                  << notification_id.c_str() << " " << std::hex << hr;
    } else {
      // We expect the notification to be removed from the action center now.
      displayed_notifications_.erase({profile_id, notification_id});

      LogCloseHistogram(CloseStatus::kSuccess);
    }
  }

  [[nodiscard]] mswr::ComPtr<winui::Notifications::IToastNotificationHistory>
  GetIToastNotificationHistory() const {
    mswr::ComPtr<winui::Notifications::IToastNotificationManagerStatics>
        toast_manager;
    HRESULT hr = CreateActivationFactory(
        RuntimeClass_Windows_UI_Notifications_ToastNotificationManager,
        IID_PPV_ARGS(&toast_manager));
    if (FAILED(hr)) {
      LogHistoryHistogram(HistoryStatus::kCreateToastNotificationManagerFailed);
      DLOG(ERROR) << "Unable to create the ToastNotificationManager "
                  << std::hex << hr;
      return nullptr;
    }

    mswr::ComPtr<winui::Notifications::IToastNotificationManagerStatics2>
        toast_manager2;
    hr = toast_manager.As(&toast_manager2);
    if (FAILED(hr)) {
      LogHistoryHistogram(HistoryStatus::kQueryToastManagerStatistics2Failed);
      DLOG(ERROR) << "Failed to get IToastNotificationManagerStatics2 "
                  << std::hex << hr;
      return nullptr;
    }

    mswr::ComPtr<winui::Notifications::IToastNotificationHistory>
        notification_history;
    hr = toast_manager2->get_History(&notification_history);
    if (FAILED(hr)) {
      LogHistoryHistogram(HistoryStatus::kGetToastHistoryFailed);
      DLOG(ERROR) << "Failed to get IToastNotificationHistory " << std::hex
                  << hr;
      return nullptr;
    }

    LogHistoryHistogram(HistoryStatus::kSuccess);

    return notification_history;
  }

  std::vector<mswr::ComPtr<winui::Notifications::IToastNotification>>
  GetDisplayedFromActionCenter() const {
    mswr::ComPtr<winui::Notifications::IToastNotificationHistory> history =
        GetIToastNotificationHistory();
    if (!history) {
      LogGetDisplayedStatus(GetDisplayedStatus::kGetToastHistoryFailed);
      DLOG(ERROR) << "Failed to get IToastNotificationHistory";
      return {};
    }

    mswr::ComPtr<winui::Notifications::IToastNotificationHistory2> history2;
    HRESULT hr = history.As(&history2);
    if (FAILED(hr)) {
      LogGetDisplayedStatus(
          GetDisplayedStatus::kQueryToastNotificationHistory2Failed);
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
      LogGetDisplayedStatus(GetDisplayedStatus::kGetHistoryWithIdFailed);
      DLOG(ERROR) << "GetHistoryWithId failed " << std::hex << hr;
      return {};
    }

    uint32_t size;
    hr = list->get_Size(&size);
    if (FAILED(hr)) {
      LogGetDisplayedStatus(GetDisplayedStatus::kGetSizeFailed);
      DLOG(ERROR) << "History get_Size call failed " << std::hex << hr;
      return {};
    }

    GetDisplayedStatus status = GetDisplayedStatus::kSuccess;

    std::vector<mswr::ComPtr<winui::Notifications::IToastNotification>>
        notifications;
    for (uint32_t index = 0; index < size; ++index) {
      mswr::ComPtr<winui::Notifications::IToastNotification> tn;
      hr = list->GetAt(index, &tn);
      if (FAILED(hr)) {
        status = GetDisplayedStatus::kSuccessWithGetAtFailure;
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
  GetNotifications() const {
    if (NotificationPlatformBridgeWinImpl::notifications_for_testing_)
      return *NotificationPlatformBridgeWinImpl::notifications_for_testing_;

    return GetDisplayedFromActionCenter();
  }

  void GetDisplayed(const std::string& profile_id,
                    bool incognito,
                    GetDisplayedNotificationsCallback callback) const {
    DCHECK(notification_task_runner_->RunsTasksInCurrentSequence());

    std::vector<mswr::ComPtr<winui::Notifications::IToastNotification>>
        notifications = GetNotifications();

    std::set<std::string> displayed_notifications;
    for (const auto& notification : notifications) {
      NotificationLaunchId launch_id(
          GetNotificationLaunchId(notification.Get()));
      if (!launch_id.is_valid()) {
        LogGetDisplayedLaunchIdStatus(
            GetDisplayedLaunchIdStatus::kDecodeLaunchIdFailed);
        DLOG(ERROR) << "Failed to decode notification ID";
        continue;
      }
      if (launch_id.profile_id() != profile_id ||
          launch_id.incognito() != incognito) {
        continue;
      }
      LogGetDisplayedLaunchIdStatus(GetDisplayedLaunchIdStatus::kSuccess);
      displayed_notifications.insert(launch_id.notification_id());
    }

    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), std::move(displayed_notifications),
                       /*supports_synchronization=*/true));
  }

  void SynchronizeNotifications() {
    DCHECK(notification_task_runner_->RunsTasksInCurrentSequence());

    if (NotificationPlatformBridgeWinImpl::
            expected_displayed_notifications_for_testing_) {
      displayed_notifications_ =
          *NotificationPlatformBridgeWinImpl::
              expected_displayed_notifications_for_testing_;
    }

    if (displayed_notifications_.empty()) {
      synchronize_displayed_notifications_timer_.Stop();
      return;
    }

    std::vector<mswr::ComPtr<winui::Notifications::IToastNotification>>
        notifications = GetNotifications();

    // Store the set of notifications displayed taking the account the
    // |profile_id| and |incognito|.
    std::set<NotificationPlatformBridgeWin::NotificationKeyType>
        displayed_notifications;
    for (const auto& notification : notifications) {
      NotificationLaunchId launch_id(
          GetNotificationLaunchId(notification.Get()));

      if (!launch_id.is_valid()) {
        continue;
      }

      displayed_notifications.insert(
          {launch_id.profile_id(), launch_id.notification_id()});
    }

    // Dispatch close event for all notifications that are not longer displayed.
    std::vector<NotificationPlatformBridgeWin::NotificationKeyType>
        key_to_remove;
    for (const auto& notification : displayed_notifications_) {
      if (!displayed_notifications.count(notification.first)) {
        HandleEvent(/*launch_id=*/notification.second,
                    NotificationOperation::kClose,
                    /*action_index=*/absl::nullopt, /*by_user=*/true);
        key_to_remove.push_back(notification.first);
      }
    }

    for (const auto& key : key_to_remove)
      displayed_notifications_.erase(key);
  }

  void InitializeOnTaskRunner() {
    DCHECK(notification_task_runner_->RunsTasksInCurrentSequence());
    LogSettingPolicyAtStartup();
    InitializeExpectedDisplayedNotification();
  }

  void InitializeExpectedDisplayedNotification() {
    DCHECK(notification_task_runner_->RunsTasksInCurrentSequence());

    std::vector<mswr::ComPtr<winui::Notifications::IToastNotification>>
        notifications = GetNotifications();

    for (const auto& notification : notifications) {
      NotificationLaunchId launch_id(
          GetNotificationLaunchId(notification.Get()));

      if (!launch_id.is_valid()) {
        continue;
      }

      displayed_notifications_[{launch_id.profile_id(),
                                launch_id.notification_id()}] = launch_id;
    }

    if (!displayed_notifications_.empty())
      MaybeStartNotificationSynchronizationTimer();
  }

  void LogSettingPolicyAtStartup() {
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
      LogGetSettingStatusStartup(GetSettingStatus::kSuccess);
      LogGetSettingPolicyStartup(ConvertSettingPolicy(setting));
    } else {
      LogGetSettingStatusStartup(GetSettingStatus::kUnknownFailure);
    }
  }

  // Test to see if the notification_helper.exe has been registered in the
  // system, either under HKCU or HKLM.
  bool IsToastActivatorRegistered() {
    base::win::RegKey key;
    std::wstring path =
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

    int status = static_cast<int>(SetReadyCallbackStatus::kSuccess);
    bool enabled = com_functions_initialized_ && activator_registered &&
                   shortcut_installed;

    if (!enabled) {
      if (!shortcut_installed) {
        status |=
            static_cast<int>(SetReadyCallbackStatus::kShortcutMisconfiguration);
      }
      if (!activator_registered) {
        status |= static_cast<int>(
            SetReadyCallbackStatus::kComServerMisconfiguration);
      }
      if (!com_functions_initialized_)
        status |= static_cast<int>(SetReadyCallbackStatus::kComNotInitialized);
    }

    LogSetReadyCallbackStatus(static_cast<SetReadyCallbackStatus>(status));

    bool success = content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), enabled));
    DCHECK(success);
  }

  void HandleEvent(NotificationLaunchId launch_id,
                   NotificationOperation operation,
                   const absl::optional<int>& action_index,
                   const absl::optional<bool>& by_user) {
    if (!launch_id.is_valid()) {
      LogHandleEventStatus(HandleEventStatus::kHandleEventLaunchIdInvalid);
      DLOG(ERROR) << "Failed to decode launch ID for operation "
                  << static_cast<int>(operation);
      return;
    }

    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&ForwardNotificationOperationOnUiThread, operation,
                       launch_id.notification_type(), launch_id.origin_url(),
                       launch_id.notification_id(), launch_id.profile_id(),
                       launch_id.incognito(), action_index,
                       /*reply=*/absl::nullopt, by_user));
    LogHandleEventStatus(HandleEventStatus::kSuccess);
  }

  absl::optional<int> ParseActionIndex(
      winui::Notifications::IToastActivatedEventArgs* args) {
    HSTRING arguments;
    HRESULT hr = args->get_Arguments(&arguments);
    if (FAILED(hr))
      return absl::nullopt;

    ScopedHString arguments_scoped(arguments);
    NotificationLaunchId launch_id(arguments_scoped.GetAsUTF8());
    return (launch_id.is_valid() && launch_id.button_index() >= 0)
               ? absl::optional<int>(launch_id.button_index())
               : absl::nullopt;
  }

  void ForwardHandleEventForTesting(
      NotificationOperation operation,
      winui::Notifications::IToastNotification* notification,
      winui::Notifications::IToastActivatedEventArgs* args,
      const absl::optional<bool>& by_user) {
    absl::optional<int> action_index = ParseActionIndex(args);
    HandleEvent(GetNotificationLaunchId(notification), operation, action_index,
                by_user);
  }

 private:
  friend class base::RefCountedThreadSafe<NotificationPlatformBridgeWinImpl>;
  friend class NotificationPlatformBridgeWin;

  ~NotificationPlatformBridgeWinImpl() {
    notification_task_runner_->DeleteSoon(FROM_HERE, image_retainer_.release());
  }

  std::wstring GetAppId() const {
    return ShellUtil::GetBrowserModelId(InstallUtil::IsPerUserInstall());
  }

  std::wstring GetTag(const std::string& notification_id,
                      const std::string& profile_id,
                      bool incognito) {
    std::string payload = base::StringPrintf(
        "%s|%s|%d", notification_id.c_str(), profile_id.c_str(), incognito);
    return base::NumberToWString(base::Hash(payload));
  }

  HRESULT OnFailed(winui::Notifications::IToastNotification* notification,
                   winui::Notifications::IToastFailedEventArgs* arguments) {
    HRESULT error_code;
    HRESULT hr = arguments->get_ErrorCode(&error_code);
    if (SUCCEEDED(hr)) {
      // Error code successfully obtained from the Action Center.
      LogOnFailedStatus(OnFailedStatus::kSuccess);
      base::UmaHistogramSparse("Notifications.Windows.DisplayFailure",
                               error_code);
      DLOG(ERROR) << "Failed to raise the toast notification, error code: "
                  << std::hex << error_code;
    } else {
      LogOnFailedStatus(OnFailedStatus::kGetErrorCodeFailed);
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
        IID_PPV_ARGS(&toast_manager));
    if (FAILED(hr)) {
      LogDisplayHistogram(DisplayStatus::kCreateToastNotificationManagerFailed);
      DLOG(ERROR) << "Unable to create the ToastNotificationManager "
                  << std::hex << hr;
      return hr;
    }

    ScopedHString application_id = ScopedHString::Create(GetAppId());
    hr = toast_manager->CreateToastNotifierWithId(application_id.get(),
                                                  &notifier_);
    if (FAILED(hr)) {
      LogDisplayHistogram(DisplayStatus::kCreateToastNotifierWithIdFailed);
      base::UmaHistogramSparse(
          "Notifications.Windows.CreateToastManagerErrorCode", hr);
      DLOG(ERROR) << "Unable to create the ToastNotifier " << std::hex << hr;
    }
    return hr;
  }

  void MaybeStartNotificationSynchronizationTimer() {
    // Avoid touching synchronize_displayed_notifications_timer_ in testing,
    // to avoid sequence checker issues at shutdown.
    if (NotificationPlatformBridgeWinImpl::notifications_for_testing_)
      return;

    if (synchronize_displayed_notifications_timer_.IsRunning())
      return;

    synchronize_displayed_notifications_timer_.Start(
        FROM_HERE, kSynchronizationInterval,
        base::BindRepeating(
            &NotificationPlatformBridgeWinImpl::SynchronizeNotifications,
            this));
  }

  static std::vector<mswr::ComPtr<winui::Notifications::IToastNotification>>*
      notifications_for_testing_;

  static std::map<NotificationPlatformBridgeWin::NotificationKeyType,
                  NotificationLaunchId>*
      expected_displayed_notifications_for_testing_;

  static winui::Notifications::IToastNotifier* notifier_for_testing_;

  const base::TimeDelta kSynchronizationInterval = base::Minutes(10);

  // Windows does not fire a close event when the notification closes. To work
  // around this, NotificationPlatformBridgeWinImpl simulates the close event by
  // periodically checking the currently displayed notifications. When
  // displaying a new notification, NotificationPlatformBridgeWinImpl stores the
  // NotificationLaunchId in the |displayed_notifications_| map.
  //
  // Every |kSynchronizationInterval| minutes, NotificationPlatformBridgeWinImpl
  // compares the currently displayed notifications against the
  // |displayed_notifications_| map. NotificationPlatformBridgeWinImpl fires the
  // close event for all notifications in |displayed_notifications_| that are no
  // longer displayed.
  base::RepeatingTimer synchronize_displayed_notifications_timer_;

  std::map<NotificationPlatformBridgeWin::NotificationKeyType,
           NotificationLaunchId>
      displayed_notifications_;

  // Whether the required functions from combase.dll have been loaded.
  const bool com_functions_initialized_;

  // The task runner running notification related tasks.
  scoped_refptr<base::SequencedTaskRunner> notification_task_runner_;

  // An object that keeps temp files alive long enough for Windows to pick up.
  std::unique_ptr<NotificationImageRetainer> image_retainer_;

  // The ToastNotifier to use to communicate with the Action Center.
  mswr::ComPtr<winui::Notifications::IToastNotifier> notifier_;
};

std::vector<mswr::ComPtr<winui::Notifications::IToastNotification>>*
    NotificationPlatformBridgeWinImpl::notifications_for_testing_ = nullptr;

std::map<NotificationPlatformBridgeWin::NotificationKeyType,
         NotificationLaunchId>* NotificationPlatformBridgeWinImpl::
    expected_displayed_notifications_for_testing_ = nullptr;

winui::Notifications::IToastNotifier*
    NotificationPlatformBridgeWinImpl::notifier_for_testing_ = nullptr;

NotificationPlatformBridgeWin::NotificationPlatformBridgeWin() {
  notification_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
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
      notification,
      ThemeServiceFactory::GetForProfile(profile)->GetColorProvider(),
      /*include_body_image=*/true, /*include_small_image=*/true,
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

void NotificationPlatformBridgeWin::SynchronizeNotificationsForTesting() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  notification_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &NotificationPlatformBridgeWinImpl::SynchronizeNotifications, impl_));
}

void NotificationPlatformBridgeWin::DisplayServiceShutDown(Profile* profile) {}

// static
bool NotificationPlatformBridgeWin::HandleActivation(
    const base::CommandLine& command_line) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  NotificationLaunchId launch_id(base::WideToUTF8(
      command_line.GetSwitchValueNative(switches::kNotificationLaunchId)));
  if (!launch_id.is_valid()) {
    LogActivationStatus(ActivationStatus::kInvalidLaunchId);
    return false;
  }

  absl::optional<std::u16string> reply;
  std::wstring inline_reply =
      command_line.GetSwitchValueNative(switches::kNotificationInlineReply);
  if (!inline_reply.empty())
    reply = base::AsString16(inline_reply);

  NotificationOperation operation;
  if (launch_id.is_for_dismiss_button())
    operation = NotificationOperation::kClose;
  else if (launch_id.is_for_context_menu())
    operation = NotificationOperation::kSettings;
  else
    operation = NotificationOperation::kClick;

  absl::optional<int> action_index;
  if (launch_id.button_index() != -1)
    action_index = launch_id.button_index();

  ForwardNotificationOperationOnUiThread(
      operation, launch_id.notification_type(), launch_id.origin_url(),
      launch_id.notification_id(), launch_id.profile_id(),
      launch_id.incognito(), std::move(action_index), reply, /*by_user=*/true);

  LogActivationStatus(ActivationStatus::kSuccess);
  return true;
}

// static
bool NotificationPlatformBridgeWin::SystemNotificationEnabled() {
  const bool enabled =
      base::FeatureList::IsEnabled(features::kNativeNotifications) &&
      base::FeatureList::IsEnabled(features::kSystemNotifications);

  // There was a Microsoft bug in Windows 10 prior to build 17134 (i.e.,
  // Version::WIN10_RS4), causing endless loops in displaying
  // notifications. It significantly amplified the memory and CPU usage.
  // Therefore, we enable Windows 10 system notification only for build 17134
  // and later. See crbug.com/882622 and crbug.com/878823 for more details.
  return base::win::GetVersion() >= base::win::Version::WIN10_RS4 && enabled;
}

void NotificationPlatformBridgeWin::ForwardHandleEventForTesting(
    NotificationOperation operation,
    winui::Notifications::IToastNotification* notification,
    winui::Notifications::IToastActivatedEventArgs* args,
    const absl::optional<bool>& by_user) {
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
  if (!notifications)
    impl_->displayed_notifications_.clear();
}

void NotificationPlatformBridgeWin::SetExpectedDisplayedNotificationsForTesting(
    std::map<NotificationPlatformBridgeWin::NotificationKeyType,
             NotificationLaunchId>* expected_displayed_notification) {
  NotificationPlatformBridgeWinImpl::
      expected_displayed_notifications_for_testing_ =
          expected_displayed_notification;
}

void NotificationPlatformBridgeWin::SetNotifierForTesting(
    winui::Notifications::IToastNotifier* notifier) {
  NotificationPlatformBridgeWinImpl::notifier_for_testing_ = notifier;
}

std::map<NotificationPlatformBridgeWin::NotificationKeyType,
         NotificationLaunchId>
NotificationPlatformBridgeWin::GetExpectedDisplayedNotificationForTesting()
    const {
  return impl_->displayed_notifications_;
}

mswr::ComPtr<winui::Notifications::IToastNotification>
NotificationPlatformBridgeWin::GetToastNotificationForTesting(
    const message_center::Notification& notification,
    const std::wstring& xml_template,
    const std::string& profile_id,
    bool incognito) {
  return impl_->GetToastNotification(notification, xml_template, profile_id,
                                     incognito);
}
