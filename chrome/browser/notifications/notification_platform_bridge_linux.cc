// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_platform_bridge_linux.h"

#include <algorithm>
#include <memory>
#include <set>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/callback_list.h"
#include "base/containers/contains.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_file.h"
#include "base/functional/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "chrome/browser/notifications/notification_display_service_impl.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/notifications/notification_operation.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/dbus/thread_linux/dbus_thread_linux.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "skia/ext/image_operations.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/message_center/public/cpp/notification.h"
#include "url/origin.h"

namespace {

// DBus name / path.
const char kFreedesktopNotificationsName[] = "org.freedesktop.Notifications";
const char kFreedesktopNotificationsPath[] = "/org/freedesktop/Notifications";

// DBus methods.
const char kMethodCloseNotification[] = "CloseNotification";
const char kMethodGetCapabilities[] = "GetCapabilities";
const char kMethodListActivatableNames[] = "ListActivatableNames";
const char kMethodNameHasOwner[] = "NameHasOwner";
const char kMethodNotify[] = "Notify";
const char kMethodStartServiceByName[] = "StartServiceByName";

// DBus signals.
const char kSignalActionInvoked[] = "ActionInvoked";
const char kSignalNotificationClosed[] = "NotificationClosed";
const char kSignalNotificationReplied[] = "NotificationReplied";

// Capabilities.
const char kCapabilityActions[] = "actions";
const char kCapabilityBody[] = "body";
const char kCapabilityBodyHyperlinks[] = "body-hyperlinks";
const char kCapabilityBodyImages[] = "body-images";
const char kCapabilityBodyMarkup[] = "body-markup";
const char kCapabilityInlineReply[] = "inline-reply";
const char kCapabilityPersistence[] = "persistence";
const char kCapabilityXKdeOriginName[] = "x-kde-origin-name";
const char kCapabilityXKdeReplyPlaceholderText[] =
    "x-kde-reply-placeholder-text";

// Button IDs.
const char kCloseButtonId[] = "close";
const char kDefaultButtonId[] = "default";
const char kInlineReplyButtonId[] = "inline-reply";
const char kSettingsButtonId[] = "settings";

// Max image size; specified in the FDO notification specification.
const int kMaxImageWidth = 200;
const int kMaxImageHeight = 100;

// Time to wait for the notification service to start.
constexpr base::TimeDelta kStartServiceTimeout = base::Seconds(1);

// Notification on-screen time, in milliseconds.
const int32_t kExpireTimeout = 25000;

// The maximum amount of characters for displaying the full origin path.
const size_t kMaxAllowedOriginLength = 28;

// Notification urgency levels, as specified in the FDO notification spec.
enum FdoUrgency {
  URGENCY_LOW = 0,
  URGENCY_NORMAL = 1,
  URGENCY_CRITICAL = 2,
};

// The values in this enumeration correspond to those of the
// Linux.NotificationPlatformBridge.InitializationStatus histogram, so
// the ordering should not be changed.  New error codes should be
// added at the end, before NUM_ITEMS.
enum class ConnectionInitializationStatusCode {
  SUCCESS = 0,
  NATIVE_NOTIFICATIONS_NOT_SUPPORTED = 1,
  MISSING_REQUIRED_CAPABILITIES = 2,
  COULD_NOT_CONNECT_TO_SIGNALS = 3,
  INCOMPATIBLE_SPEC_VERSION = 4,  // DEPRECATED
  NUM_ITEMS
};

std::u16string CreateNotificationTitle(
    const message_center::Notification& notification) {
  std::u16string title;
  if (notification.type() == message_center::NOTIFICATION_TYPE_PROGRESS) {
    title += base::FormatPercent(notification.progress());
    title += u" - ";
  }
  title += notification.title();
  return title;
}

void EscapeUnsafeCharacters(std::string* message) {
  // Canonical's notification development guidelines recommends only
  // escaping the '&', '<', and '>' characters:
  // https://wiki.ubuntu.com/NotificationDevelopmentGuidelines
  base::ReplaceChars(*message, "&", "&amp;", message);
  base::ReplaceChars(*message, "<", "&lt;", message);
  base::ReplaceChars(*message, ">", "&gt;", message);
}

int NotificationPriorityToFdoUrgency(int priority) {
  switch (priority) {
    case message_center::MIN_PRIORITY:
    case message_center::LOW_PRIORITY:
      return URGENCY_LOW;
    case message_center::HIGH_PRIORITY:
    case message_center::MAX_PRIORITY:
      return URGENCY_CRITICAL;
    default:
      NOTREACHED_IN_MIGRATION();
      [[fallthrough]];
    case message_center::DEFAULT_PRIORITY:
      return URGENCY_NORMAL;
  }
}

// Constrain |image|'s size to |kMaxImageWidth|x|kMaxImageHeight|. If
// the image does not need to be resized, or the image is empty,
// returns |image| directly.
gfx::Image ResizeImageToFdoMaxSize(const gfx::Image& image) {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (image.IsEmpty())
    return image;
  int width = image.Width();
  int height = image.Height();
  if (width <= kMaxImageWidth && height <= kMaxImageHeight) {
    return image;
  }
  const SkBitmap* image_bitmap = image.ToSkBitmap();
  double scale = std::min(static_cast<double>(kMaxImageWidth) / width,
                          static_cast<double>(kMaxImageHeight) / height);
  width = std::clamp<int>(scale * width, 1, kMaxImageWidth);
  height = std::clamp<int>(scale * height, 1, kMaxImageHeight);
  return gfx::Image(
      gfx::ImageSkia::CreateFrom1xBitmap(skia::ImageOperations::Resize(
          *image_bitmap, skia::ImageOperations::RESIZE_LANCZOS3, width,
          height)));
}

bool ShouldAddCloseButton(const std::string& server_name,
                          const base::Version& server_version) {
  // Cinnamon doesn't add a close button on notifications.  With eg. calendar
  // notifications, which are stay-on-screen, this can lead to a situation where
  // the only way to dismiss a notification is to click on it, which would
  // create an unwanted web navigation.  For this reason, manually add a close
  // button (https://crbug.com/804637).  Cinnamon 3.8.0 adds a close button
  // (https://github.com/linuxmint/Cinnamon/blob/8717fa/debian/changelog#L1075),
  // so exclude versions that provide one already.
  return server_name == "cinnamon" && server_version.IsValid() &&
         server_version.CompareToWildcardString("3.8.0") < 0;
}

bool ShouldMarkPersistentNotificationsAsCritical(
    const std::string& server_name) {
  // Gnome-based desktops intentionally disregard the notification timeout
  // and hide a notification automatically unless it is marked as critical.
  // https://github.com/linuxmint/Cinnamon/issues/7179
  // For this reason, we mark a notification that should not time out as
  // critical unless we are on KDE Plasma which follows the notification spec.
  return server_name != "Plasma";
}

void ForwardNotificationOperationOnUiThread(
    NotificationOperation operation,
    NotificationHandler::Type notification_type,
    const GURL& origin,
    const std::string& notification_id,
    const std::optional<int>& action_index,
    const std::optional<bool>& by_user,
    const std::optional<std::u16string>& reply,
    const std::string& profile_id,
    bool is_incognito) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Profile ID can be empty for system notifications, which are not bound to a
  // profile, but system notifications are transient and thus not handled by
  // this NotificationPlatformBridge.
  // When transient notifications are supported, this should route the
  // notification response to the system NotificationDisplayService.
  DCHECK(!profile_id.empty());

  g_browser_process->profile_manager()->LoadProfile(
      NotificationPlatformBridge::GetProfileBaseNameFromProfileId(profile_id),
      is_incognito,
      base::BindOnce(&NotificationDisplayServiceImpl::ProfileLoadedCallback,
                     operation, notification_type, origin, notification_id,
                     action_index, reply, by_user));
}

// Writes `data` to a new temporary file and returns the temporary file
base::ScopedTempFile WriteDataToTmpFile(
    const scoped_refptr<base::RefCountedMemory>& data) {
  if (data->size() == 0) {
    return {};
  }

  base::ScopedTempFile file;
  if (!file.Create()) {
    return file;
  }

  if (!base::WriteFile(file.path(), *data)) {
    return {};
  }
  return file;
}

bool CheckNotificationsNameHasOwnerOrIsActivatable(dbus::Bus* bus) {
  dbus::ObjectProxy* dbus_proxy =
      bus->GetObjectProxy(DBUS_SERVICE_DBUS, dbus::ObjectPath(DBUS_PATH_DBUS));
  dbus::MethodCall name_has_owner_call(DBUS_INTERFACE_DBUS,
                                       kMethodNameHasOwner);
  dbus::MessageWriter writer(&name_has_owner_call);
  writer.AppendString(kFreedesktopNotificationsName);
  std::unique_ptr<dbus::Response> name_has_owner_response =
      dbus_proxy
          ->CallMethodAndBlock(&name_has_owner_call,
                               dbus::ObjectProxy::TIMEOUT_USE_DEFAULT)
          .value_or(nullptr);
  dbus::MessageReader owner_reader(name_has_owner_response.get());
  bool owned = false;
  if (name_has_owner_response && owner_reader.PopBool(&owned) && owned)
    return true;

  // If the service currently isn't running, maybe it is activatable.
  dbus::MethodCall list_activatable_names_call(DBUS_INTERFACE_DBUS,
                                               kMethodListActivatableNames);
  std::unique_ptr<dbus::Response> list_activatable_names_response =
      dbus_proxy
          ->CallMethodAndBlock(&list_activatable_names_call,
                               dbus::ObjectProxy::TIMEOUT_USE_DEFAULT)
          .value_or(nullptr);
  if (list_activatable_names_response) {
    dbus::MessageReader reader(list_activatable_names_response.get());
    std::vector<std::string> activatable_names;
    reader.PopArrayOfStrings(&activatable_names);
    if (base::Contains(activatable_names, kFreedesktopNotificationsName)) {
      dbus::MethodCall start_service_call(DBUS_INTERFACE_DBUS,
                                          kMethodStartServiceByName);
      dbus::MessageWriter start_service_writer(&start_service_call);
      start_service_writer.AppendString(kFreedesktopNotificationsName);
      start_service_writer.AppendUint32(/*flags=*/0);
      auto start_service_response =
          dbus_proxy
              ->CallMethodAndBlock(&start_service_call,
                                   kStartServiceTimeout.InMilliseconds())
              .value_or(nullptr);
      if (!start_service_response)
        return false;
      dbus::MessageReader start_service_reader(start_service_response.get());
      uint32_t start_service_reply = 0;
      if (start_service_reader.PopUint32(&start_service_reply) &&
          (start_service_reply == DBUS_START_REPLY_SUCCESS ||
           start_service_reply == DBUS_START_REPLY_ALREADY_RUNNING)) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace

// static
std::unique_ptr<NotificationPlatformBridge>
NotificationPlatformBridge::Create() {
  return std::make_unique<NotificationPlatformBridgeLinux>();
}

// static
bool NotificationPlatformBridge::CanHandleType(
    NotificationHandler::Type notification_type) {
  return notification_type != NotificationHandler::Type::TRANSIENT;
}

class NotificationPlatformBridgeLinuxImpl
    : public NotificationPlatformBridge,
      public base::RefCountedThreadSafe<NotificationPlatformBridgeLinuxImpl> {
 public:
  explicit NotificationPlatformBridgeLinuxImpl(scoped_refptr<dbus::Bus> bus)
      : bus_(bus) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    task_runner_ = dbus_thread_linux::GetTaskRunner();
    // base::Unretained(this) is safe here as this object owns
    // |on_app_terminating_subscription_| and the callback won't be invoked
    // after the subscription is destroyed.
    on_app_terminating_subscription_ =
        browser_shutdown::AddAppTerminatingCallback(base::BindOnce(
            &NotificationPlatformBridgeLinuxImpl::OnAppTerminating,
            base::Unretained(this)));
  }
  NotificationPlatformBridgeLinuxImpl(
      const NotificationPlatformBridgeLinuxImpl&) = delete;
  NotificationPlatformBridgeLinuxImpl& operator=(
      const NotificationPlatformBridgeLinuxImpl&) = delete;

  // InitOnTaskRunner() cannot be posted from within the constructor
  // because of a race condition.  The reference count for |this|
  // starts out as 0.  Posting the Init task would increment the count
  // to 1.  If the task finishes before the constructor returns, the
  // count will go to 0 and the object would be prematurely
  // destructed.
  void Init() {
    product_logo_png_bytes_ =
        gfx::Image(*ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
                       IDR_PRODUCT_LOGO_64))
            .As1xPNGBytes();
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&NotificationPlatformBridgeLinuxImpl::InitOnTaskRunner,
                       this));
  }

  void Display(
      NotificationHandler::Type notification_type,
      Profile* profile,
      const message_center::Notification& notification,
      std::unique_ptr<NotificationCommon::Metadata> metadata) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    // Make a deep copy of the notification as its resources cannot safely
    // be passed between threads.
    auto notification_copy = message_center::Notification::DeepCopy(
        notification,
        ThemeServiceFactory::GetForProfile(profile)->GetColorProvider(),
        body_images_supported_.value(),
        /*include_small_image=*/false, /*include_icon_images=*/false);

    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &NotificationPlatformBridgeLinuxImpl::DisplayOnTaskRunner, this,
            notification_type, GetProfileId(profile), profile->IsOffTheRecord(),
            std::move(notification_copy)));
  }

  void Close(Profile* profile, const std::string& notification_id) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&NotificationPlatformBridgeLinuxImpl::CloseOnTaskRunner,
                       this, GetProfileId(profile), notification_id));
  }

  void GetDisplayed(Profile* profile,
                    GetDisplayedNotificationsCallback callback) const override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &NotificationPlatformBridgeLinuxImpl::GetDisplayedOnTaskRunner,
            this, GetProfileId(profile), profile->IsOffTheRecord(),
            std::move(callback)));
  }

  void GetDisplayedForOrigin(
      Profile* profile,
      const GURL& origin,
      GetDisplayedNotificationsCallback callback) const override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&NotificationPlatformBridgeLinuxImpl::
                           GetDisplayedForOriginOnTaskRunner,
                       this, GetProfileId(profile), profile->IsOffTheRecord(),
                       origin, std::move(callback)));
  }

  void SetReadyCallback(NotificationBridgeReadyCallback callback) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (connected_.has_value()) {
      std::move(callback).Run(connected_.value());
    } else {
      on_connected_callbacks_.push_back(std::move(callback));
    }
  }

  void DisplayServiceShutDown(Profile* profile) override {}

  void CleanUp() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &NotificationPlatformBridgeLinuxImpl::CleanUpOnTaskRunner, this));
  }

 private:
  friend class base::RefCountedThreadSafe<NotificationPlatformBridgeLinuxImpl>;

  struct NotificationData {
    NotificationData(NotificationHandler::Type notification_type,
                     const std::string& notification_id,
                     const std::string& profile_id,
                     bool is_incognito,
                     const GURL& origin_url)
        : notification_type(notification_type),
          notification_id(notification_id),
          profile_id(profile_id),
          is_incognito(is_incognito),
          origin_url(origin_url) {}

    // The ID used by the notification server.  Will be 0 until the
    // first "Notify" message completes.
    uint32_t dbus_id = 0;

    // Same parameters used by NotificationPlatformBridge::Display().
    NotificationHandler::Type notification_type;
    const std::string notification_id;
    const std::string profile_id;
    const bool is_incognito;

    // A copy of the origin_url from the underlying
    // message_center::Notification.  Used to pass back to
    // NotificationDisplayService.
    const GURL origin_url;

    // Used to keep track of the IDs of the buttons currently displayed
    // on this notification.  The valid range of action IDs is
    // [action_start, action_end).
    size_t action_start = 0;
    size_t action_end = 0;

    // Temporary resource files associated with the notification that
    // should be cleaned up when the notification is closed or on
    // shutdown.
    std::vector<base::ScopedTempFile> resource_files;
  };

  ~NotificationPlatformBridgeLinuxImpl() override {
    // TODO(crbug.com/41398799): This should DCHECK, but doing so makes tests in
    // components unrelated to notifications flaky. Log instead so that those
    // tests can retain test coverage.
    DLOG_IF(ERROR, !clean_up_on_task_runner_called_) << "Not cleaned up";
  }

  void OnAppTerminating() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    // The browser process is about to exit.  Post the CleanUp() task
    // while we still can.
    CleanUp();
  }

  void SetBodyImagesSupported(bool body_images_supported) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    body_images_supported_ = body_images_supported;
  }

  // Sets up the D-Bus connection.
  void InitOnTaskRunner() {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    // |bus_| may be non-null in unit testing where a fake bus is used.
    if (!bus_) {
      dbus::Bus::Options bus_options;
      bus_options.bus_type = dbus::Bus::SESSION;
      bus_options.connection_type = dbus::Bus::PRIVATE;
      bus_options.dbus_task_runner = task_runner_;
      bus_ = base::MakeRefCounted<dbus::Bus>(bus_options);
    }

    if (!CheckNotificationsNameHasOwnerOrIsActivatable(bus_.get())) {
      OnConnectionInitializationFinishedOnTaskRunner(
          ConnectionInitializationStatusCode::
              NATIVE_NOTIFICATIONS_NOT_SUPPORTED);
      return;
    }
    notification_proxy_ =
        bus_->GetObjectProxy(kFreedesktopNotificationsName,
                             dbus::ObjectPath(kFreedesktopNotificationsPath));

    dbus::MethodCall get_capabilities_call(kFreedesktopNotificationsName,
                                           kMethodGetCapabilities);
    std::unique_ptr<dbus::Response> capabilities_response =
        notification_proxy_
            ->CallMethodAndBlock(&get_capabilities_call,
                                 dbus::ObjectProxy::TIMEOUT_USE_DEFAULT)
            .value_or(nullptr);
    if (capabilities_response) {
      dbus::MessageReader reader(capabilities_response.get());
      std::vector<std::string> capabilities;
      reader.PopArrayOfStrings(&capabilities);
      for (const std::string& capability : capabilities)
        capabilities_.insert(capability);
    }
    if (!base::Contains(capabilities_, kCapabilityBody) ||
        !base::Contains(capabilities_, kCapabilityActions)) {
      OnConnectionInitializationFinishedOnTaskRunner(
          ConnectionInitializationStatusCode::MISSING_REQUIRED_CAPABILITIES);
      return;
    }
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &NotificationPlatformBridgeLinuxImpl::SetBodyImagesSupported, this,
            base::Contains(capabilities_, kCapabilityBodyImages)));

    dbus::MethodCall get_server_information_call(kFreedesktopNotificationsName,
                                                 "GetServerInformation");
    std::unique_ptr<dbus::Response> server_information_response =
        notification_proxy_
            ->CallMethodAndBlock(&get_server_information_call,
                                 dbus::ObjectProxy::TIMEOUT_USE_DEFAULT)
            .value_or(nullptr);
    if (server_information_response) {
      dbus::MessageReader reader(server_information_response.get());
      reader.PopString(&server_name_);
      std::string server_version;
      reader.PopString(&server_version);  // Vendor
      reader.PopString(&server_version);  // Server version
      server_version_ = base::Version(server_version);
    }

    DCHECK(!connect_signals_in_progress_);
    connect_signals_in_progress_ = true;
    connected_signals_barrier_ = base::BarrierClosure(
        3, base::BindOnce(&NotificationPlatformBridgeLinuxImpl::
                              OnConnectionInitializationFinishedOnTaskRunner,
                          this, ConnectionInitializationStatusCode::SUCCESS));
    notification_proxy_->ConnectToSignal(
        kFreedesktopNotificationsName, kSignalActionInvoked,
        base::BindRepeating(
            &NotificationPlatformBridgeLinuxImpl::OnActionInvoked, this),
        base::BindOnce(&NotificationPlatformBridgeLinuxImpl::OnSignalConnected,
                       this));
    notification_proxy_->ConnectToSignal(
        kFreedesktopNotificationsName, kSignalNotificationClosed,
        base::BindRepeating(
            &NotificationPlatformBridgeLinuxImpl::OnNotificationClosed, this),
        base::BindOnce(&NotificationPlatformBridgeLinuxImpl::OnSignalConnected,
                       this));
    notification_proxy_->ConnectToSignal(
        kFreedesktopNotificationsName, kSignalNotificationReplied,
        base::BindRepeating(
            &NotificationPlatformBridgeLinuxImpl::OnNotificationReplied, this),
        base::BindOnce(&NotificationPlatformBridgeLinuxImpl::OnSignalConnected,
                       this));
  }

  void CleanUpOnTaskRunner() {
    if (connect_signals_in_progress_) {
      // Connecting to a signal is still in progress. Defer cleanup task.
      should_cleanup_on_signal_connected_ = true;
      return;
    }

    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    notification_proxy_ = nullptr;
    if (bus_)
      bus_->ShutdownAndBlock();
    bus_ = nullptr;
    product_logo_png_bytes_ = nullptr;
    product_logo_file_.Reset();
    product_logo_file_watcher_.reset();
    notifications_.clear();
    clean_up_on_task_runner_called_ = true;
  }

  // Makes the "Notify" call to D-Bus.
  void DisplayOnTaskRunner(
      NotificationHandler::Type notification_type,
      const std::string& profile_id,
      bool is_incognito,
      std::unique_ptr<message_center::Notification> notification) {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    NotificationData* data =
        FindNotificationData(notification->id(), profile_id, is_incognito);
    if (data) {
      // Update an existing notification.
      data->notification_type = notification_type;
      data->resource_files.clear();
    } else {
      // Send the notification for the first time.
      data = new NotificationData(notification_type, notification->id(),
                                  profile_id, is_incognito,
                                  notification->origin_url());
      notifications_.emplace(data, base::WrapUnique(data));
    }

    dbus::MethodCall method_call(kFreedesktopNotificationsName, kMethodNotify);
    dbus::MessageWriter writer(&method_call);

    // app_name
    writer.AppendString(l10n_util::GetStringUTF8(IDS_PRODUCT_NAME));

    writer.AppendUint32(data->dbus_id);

    // app_icon
    if (!product_logo_file_) {
      RewriteProductLogoFile();
    }
    writer.AppendString(product_logo_file_
                            ? "file://" + product_logo_file_.path().value()
                            : "");

    writer.AppendString(
        base::UTF16ToUTF8(CreateNotificationTitle(*notification)));

    std::string context_display_text;
    bool linkify_context_if_possible = false;
    if (notification->UseOriginAsContextMessage()) {
      context_display_text =
          base::UTF16ToUTF8(url_formatter::FormatUrlForSecurityDisplay(
              notification->origin_url(),
              url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS));
      if (context_display_text.size() > kMaxAllowedOriginLength) {
        std::string domain_and_registry =
            net::registry_controlled_domains::GetDomainAndRegistry(
                notification->origin_url(),
                net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
        // localhost, raw IPs etc. are not handled by GetDomainAndRegistry.
        if (!domain_and_registry.empty()) {
          context_display_text = domain_and_registry;
        }
      }
      linkify_context_if_possible = true;
    } else {
      context_display_text = base::UTF16ToUTF8(notification->context_message());
    }

    const bool has_support_for_kde_origin_name =
        base::Contains(capabilities_, kCapabilityXKdeOriginName);

    std::ostringstream body;
    if (base::Contains(capabilities_, kCapabilityBody)) {
      const bool body_markup =
          base::Contains(capabilities_, kCapabilityBodyMarkup);

      if (!has_support_for_kde_origin_name) {
        if (body_markup) {
          EscapeUnsafeCharacters(&context_display_text);
        }

        if (linkify_context_if_possible) {
          if (base::Contains(capabilities_, kCapabilityBodyHyperlinks)) {
            body << "<a href=\""
                 << base::EscapeForHTML(notification->origin_url().spec())
                 << "\">" << context_display_text << "</a>\n\n";
          } else {
            body << context_display_text << "\n\n";
          }
        } else if (!context_display_text.empty()) {
          body << context_display_text << "\n\n";
        }
      }

      std::string message = base::UTF16ToUTF8(notification->message());
      if (body_markup)
        EscapeUnsafeCharacters(&message);
      if (!message.empty())
        body << message << "\n";

      if (notification->type() == message_center::NOTIFICATION_TYPE_MULTIPLE) {
        for (const auto& item : notification->items()) {
          const std::string item_title = base::UTF16ToUTF8(item.title());
          const std::string item_message = base::UTF16ToUTF8(item.message());
          // TODO(peter): Figure out the right way to internationalize
          // this for RTL languages.
          if (body_markup)
            body << "<b>" << item_title << "</b> " << item_message << "\n";
          else
            body << item_title << " - " << item_message << "\n";
        }
      } else if (notification->type() ==
                     message_center::NOTIFICATION_TYPE_IMAGE &&
                 base::Contains(capabilities_, kCapabilityBodyImages)) {
        auto image_file = WriteDataToTmpFile(
            ResizeImageToFdoMaxSize(notification->image()).As1xPNGBytes());
        if (image_file) {
          body << "<img src=\""
               << "file://" + base::EscapePath(image_file.path().value())
               << "\" alt=\"\"/>\n";
          data->resource_files.push_back(std::move(image_file));
        }
      }
    }
    std::string body_str = body.str();
    base::TrimString(body_str, "\n", &body_str);
    writer.AppendString(body_str);

    // Even-indexed elements in this vector are action IDs passed back to
    // us in OnActionInvoked().  Odd-indexed ones contain the button text.
    std::vector<std::string> actions;
    std::optional<std::u16string> inline_reply_placeholder;
    if (base::Contains(capabilities_, kCapabilityActions)) {
      const bool has_support_for_inline_reply =
          base::Contains(capabilities_, kCapabilityInlineReply);

      data->action_start = data->action_end;

      for (const auto& button_info : notification->buttons()) {
        const std::string label = base::UTF16ToUTF8(button_info.title);

        if (has_support_for_inline_reply &&
            connected_to_notification_replied_signal_ &&
            button_info.placeholder) {
          // There can only be one inline-reply action
          if (inline_reply_placeholder) {
            continue;
          }

          actions.push_back(kInlineReplyButtonId);
          actions.push_back(label);

          inline_reply_placeholder = button_info.placeholder;
          continue;
        }
        // FDO notification buttons can contain either an icon or a label,
        // but not both, and the type of all buttons must be the same (all
        // labels or all icons), so always use labels.
        const std::string id = base::NumberToString(data->action_end++);
        actions.push_back(id);
        actions.push_back(label);
      }
      // Special case: the id "default" will not add a button, but
      // instead makes the entire notification clickable.
      actions.push_back(kDefaultButtonId);
      actions.push_back("Activate");
      // Always add a settings button for web notifications.
      if (notification->should_show_settings_button()) {
        actions.push_back(kSettingsButtonId);
        actions.push_back(
            l10n_util::GetStringUTF8(IDS_NOTIFICATION_BUTTON_SETTINGS));
      }
      if (ShouldAddCloseButton(server_name_, server_version_)) {
        actions.push_back(kCloseButtonId);
        actions.push_back(
            l10n_util::GetStringUTF8(IDS_NOTIFICATION_BUTTON_CLOSE));
      }
    }
    writer.AppendArrayOfStrings(actions);

    dbus::MessageWriter hints_writer(nullptr);
    writer.OpenArray("{sv}", &hints_writer);
    dbus::MessageWriter urgency_writer(nullptr);
    hints_writer.OpenDictEntry(&urgency_writer);
    urgency_writer.AppendString("urgency");
    uint32_t urgency =
        notification->never_timeout() &&
                ShouldMarkPersistentNotificationsAsCritical(server_name_)
            ? URGENCY_CRITICAL
            : NotificationPriorityToFdoUrgency(notification->priority());
    urgency_writer.AppendVariantOfUint32(urgency);
    hints_writer.CloseContainer(&urgency_writer);

    if (notification->silent()) {
      dbus::MessageWriter suppress_sound_writer(nullptr);
      hints_writer.OpenDictEntry(&suppress_sound_writer);
      suppress_sound_writer.AppendString("suppress-sound");
      suppress_sound_writer.AppendVariantOfBool(true);
      hints_writer.CloseContainer(&suppress_sound_writer);
    }

    std::unique_ptr<base::Environment> env = base::Environment::Create();
    base::FilePath desktop_file(chrome::GetDesktopName(env.get()));
    const char kDesktopFileSuffix[] = ".desktop";
    DCHECK(base::EndsWith(desktop_file.value(), kDesktopFileSuffix,
                          base::CompareCase::SENSITIVE));
    desktop_file = desktop_file.RemoveFinalExtension();
    dbus::MessageWriter desktop_entry_writer(nullptr);
    hints_writer.OpenDictEntry(&desktop_entry_writer);
    desktop_entry_writer.AppendString("desktop-entry");
    desktop_entry_writer.AppendVariantOfString(desktop_file.value());
    hints_writer.CloseContainer(&desktop_entry_writer);

    auto icon_file = WriteDataToTmpFile(
        gfx::Image(notification->icon().Rasterize(nullptr)).As1xPNGBytes());
    if (icon_file) {
      for (const std::string& hint_name : {"image_path", "image-path"}) {
        dbus::MessageWriter image_path_writer(nullptr);
        hints_writer.OpenDictEntry(&image_path_writer);
        image_path_writer.AppendString(hint_name);
        image_path_writer.AppendVariantOfString(icon_file.path().value());
        hints_writer.CloseContainer(&image_path_writer);
      }
      data->resource_files.push_back(std::move(icon_file));
    }

    if (has_support_for_kde_origin_name && !context_display_text.empty()) {
      dbus::MessageWriter kde_origin_name_writer(nullptr);
      hints_writer.OpenDictEntry(&kde_origin_name_writer);
      kde_origin_name_writer.AppendString(kCapabilityXKdeOriginName);
      kde_origin_name_writer.AppendVariantOfString(context_display_text);
      hints_writer.CloseContainer(&kde_origin_name_writer);
    }

    if (inline_reply_placeholder) {
      dbus::MessageWriter inline_reply_writer(nullptr);
      hints_writer.OpenDictEntry(&inline_reply_writer);
      inline_reply_writer.AppendString(kCapabilityXKdeReplyPlaceholderText);
      inline_reply_writer.AppendVariantOfString(
          base::UTF16ToUTF8(inline_reply_placeholder.value()));
      hints_writer.CloseContainer(&inline_reply_writer);
    }

    writer.CloseContainer(&hints_writer);

    const int32_t kExpireTimeoutDefault = -1;
    const int32_t kExpireTimeoutNever = 0;
    writer.AppendInt32(
        notification->never_timeout()
            ? kExpireTimeoutNever
            : base::Contains(capabilities_, kCapabilityPersistence)
                  ? kExpireTimeoutDefault
                  : kExpireTimeout);

    std::unique_ptr<dbus::Response> response =
        notification_proxy_
            ->CallMethodAndBlock(&method_call,
                                 dbus::ObjectProxy::TIMEOUT_USE_DEFAULT)
            .value_or(nullptr);
    if (response) {
      dbus::MessageReader reader(response.get());
      reader.PopUint32(&data->dbus_id);
    }
    if (!data->dbus_id) {
      // There was some sort of error with creating the notification.
      notifications_.erase(data);
    }
  }

  // Makes the "CloseNotification" call to D-Bus.
  void CloseOnTaskRunner(const std::string& profile_id,
                         const std::string& notification_id) {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    std::vector<NotificationData*> to_erase;
    for (const auto& pair : notifications_) {
      NotificationData* data = pair.first;
      if (data->notification_id == notification_id &&
          data->profile_id == profile_id) {
        dbus::MethodCall method_call(kFreedesktopNotificationsName,
                                     kMethodCloseNotification);
        dbus::MessageWriter writer(&method_call);
        writer.AppendUint32(data->dbus_id);
        // TODO: resolve if std::ignore is ok.
        std::ignore = notification_proxy_->CallMethodAndBlock(
            &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT);
        to_erase.push_back(data);
      }
    }
    for (NotificationData* data : to_erase)
      notifications_.erase(data);
  }

  void GetDisplayedOnTaskRunner(
      const std::string& profile_id,
      bool incognito,
      GetDisplayedNotificationsCallback callback) const {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    std::set<std::string> displayed;
    for (const auto& pair : notifications_) {
      NotificationData* data = pair.first;
      if (data->profile_id == profile_id && data->is_incognito == incognito) {
        displayed.insert(data->notification_id);
      }
    }
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), std::move(displayed), true));
  }

  void GetDisplayedForOriginOnTaskRunner(
      const std::string& profile_id,
      bool incognito,
      const GURL& origin,
      GetDisplayedNotificationsCallback callback) const {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    std::set<std::string> displayed;
    for (const auto& pair : notifications_) {
      NotificationData* data = pair.first;
      if (data->profile_id == profile_id && data->is_incognito == incognito &&
          url::IsSameOriginWith(data->origin_url, origin)) {
        displayed.insert(data->notification_id);
      }
    }
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), std::move(displayed), true));
  }

  NotificationData* FindNotificationData(const std::string& notification_id,
                                         const std::string& profile_id,
                                         bool is_incognito) {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    for (const auto& pair : notifications_) {
      NotificationData* data = pair.first;
      if (data->notification_id == notification_id &&
          data->profile_id == profile_id &&
          data->is_incognito == is_incognito) {
        return data;
      }
    }

    return nullptr;
  }

  NotificationData* FindNotificationDataWithDBusId(uint32_t dbus_id) {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    DCHECK(dbus_id);
    for (const auto& pair : notifications_) {
      NotificationData* data = pair.first;
      if (data->dbus_id == dbus_id)
        return data;
    }

    return nullptr;
  }

  void ForwardNotificationOperation(
      const base::Location& location,
      NotificationData* data,
      NotificationOperation operation,
      const std::optional<int>& action_index,
      const std::optional<bool>& by_user,
      const std::optional<std::u16string>& reply) {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    content::GetUIThreadTaskRunner({})->PostTask(
        location,
        base::BindOnce(ForwardNotificationOperationOnUiThread, operation,
                       data->notification_type, data->origin_url,
                       data->notification_id, action_index, by_user, reply,
                       data->profile_id, data->is_incognito));
  }

  void OnActionInvoked(dbus::Signal* signal) {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    dbus::MessageReader reader(signal);
    uint32_t dbus_id;
    if (!reader.PopUint32(&dbus_id) || !dbus_id)
      return;
    std::string action;
    if (!reader.PopString(&action))
      return;

    NotificationData* data = FindNotificationDataWithDBusId(dbus_id);
    if (!data)
      return;

    if (action == kDefaultButtonId) {
      ForwardNotificationOperation(
          FROM_HERE, data, NotificationOperation::kClick,
          std::nullopt /* action_index */, std::nullopt /* by_user */,
          std::nullopt /* reply */);
    } else if (action == kSettingsButtonId) {
      ForwardNotificationOperation(
          FROM_HERE, data, NotificationOperation::kSettings,
          std::nullopt /* action_index */, std::nullopt /* by_user */,
          std::nullopt /* reply */);
    } else if (action == kCloseButtonId) {
      ForwardNotificationOperation(
          FROM_HERE, data, NotificationOperation::kClose,
          std::nullopt /* action_index */, true /* by_user */,
          std::nullopt /* reply */);
      CloseOnTaskRunner(data->profile_id, data->notification_id);
    } else {
      size_t id;
      if (!base::StringToSizeT(action, &id))
        return;
      size_t n_buttons = data->action_end - data->action_start;
      size_t id_zero_based = id - data->action_start;
      if (id_zero_based >= n_buttons)
        return;
      ForwardNotificationOperation(
          FROM_HERE, data, NotificationOperation::kClick, id_zero_based,
          std::nullopt /* by_user */, std::nullopt /* reply */);
    }
  }

  void OnNotificationReplied(dbus::Signal* signal) {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    dbus::MessageReader reader(signal);
    uint32_t dbus_id;
    if (!reader.PopUint32(&dbus_id) || !dbus_id)
      return;
    std::string reply;
    if (!reader.PopString(&reply))
      return;

    NotificationData* data = FindNotificationDataWithDBusId(dbus_id);
    if (!data)
      return;

    ForwardNotificationOperation(FROM_HERE, data, NotificationOperation::kClick,
                                 std::nullopt /* action_index */,
                                 std::nullopt /* by_user */,
                                 base::UTF8ToUTF16(reply));
  }

  void OnNotificationClosed(dbus::Signal* signal) {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    dbus::MessageReader reader(signal);
    uint32_t dbus_id;
    if (!reader.PopUint32(&dbus_id) || !dbus_id)
      return;

    NotificationData* data = FindNotificationDataWithDBusId(dbus_id);
    if (!data)
      return;

    // TODO(peter): Can we support |by_user| appropriately here?
    ForwardNotificationOperation(FROM_HERE, data, NotificationOperation::kClose,
                                 std::nullopt /* action_index */,
                                 true /* by_user */, std::nullopt /* reply */);
    notifications_.erase(data);
  }

  // Called once the connection has been set up (or not).  |success|
  // indicates the connection is ready to use.
  void OnConnectionInitializationFinishedOnUiThread(bool success) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    connected_ = success;
    for (auto& callback : on_connected_callbacks_)
      std::move(callback).Run(success);
    on_connected_callbacks_.clear();
    if (!success)
      CleanUp();
  }

  void OnConnectionInitializationFinishedOnTaskRunner(
      ConnectionInitializationStatusCode status) {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    bool success = status == ConnectionInitializationStatusCode::SUCCESS;

    // Note: Not all code paths set connect_signals_in_progress_ to true!
    connect_signals_in_progress_ = false;
    if (should_cleanup_on_signal_connected_) {
      // Mark as fail, so that observers don't think we're initialized.
      success = false;
      CleanUpOnTaskRunner();
    }

    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&NotificationPlatformBridgeLinuxImpl::
                           OnConnectionInitializationFinishedOnUiThread,
                       this, success));
  }

  void OnSignalConnected(const std::string& interface_name,
                         const std::string& signal_name,
                         bool success) {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    const bool isNotificationRepliedSignal =
        (signal_name == kSignalNotificationReplied);
    if (isNotificationRepliedSignal) {
      connected_to_notification_replied_signal_ = success;
    } else if (!success) {
      OnConnectionInitializationFinishedOnTaskRunner(
          ConnectionInitializationStatusCode::COULD_NOT_CONNECT_TO_SIGNALS);
      return;
    }
    connected_signals_barrier_.Run();
  }

  void OnProductLogoFileChanged(const base::FilePath& path, bool error) {
    // |error| should always be false on Linux.
    DCHECK(!error);
    // This callback runs whenever the file is deleted or modified.
    // In either case, we want to rewrite the file.
    product_logo_file_.Reset();
    product_logo_file_watcher_.reset();
  }

  void RewriteProductLogoFile() {
    product_logo_file_watcher_.reset();
    product_logo_file_ = WriteDataToTmpFile(product_logo_png_bytes_);
    if (!product_logo_file_) {
      return;
    }
    // Temporary files may periodically get cleaned up on Linux.
    // Watch for file deletion and rewrite the file in case we have a
    // long-running Chrome process.
    product_logo_file_watcher_ = std::make_unique<base::FilePathWatcher>();
    if (!product_logo_file_watcher_->Watch(
            product_logo_file_.path(),
            base::FilePathWatcher::Type::kNonRecursive,
            base::BindRepeating(
                &NotificationPlatformBridgeLinuxImpl::OnProductLogoFileChanged,
                this))) {
      product_logo_file_.Reset();
      product_logo_file_watcher_.reset();
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  // Members used only on the UI thread.

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::CallbackListSubscription on_app_terminating_subscription_;

  // State necessary for OnConnectionInitializationFinished() and
  // SetReadyCallback().
  std::optional<bool> connected_;
  std::vector<NotificationBridgeReadyCallback> on_connected_callbacks_;

  // Notification servers very rarely have the 'body-images'
  // capability, so try to avoid an image copy if possible.
  std::optional<bool> body_images_supported_;

  //////////////////////////////////////////////////////////////////////////////
  // Members used only on the task runner thread.

  scoped_refptr<dbus::Bus> bus_;

  raw_ptr<dbus::ObjectProxy, FlakyDanglingUntriaged> notification_proxy_ =
      nullptr;

  std::unordered_set<std::string> capabilities_;

  std::string server_name_;
  base::Version server_version_;

  base::RepeatingClosure connected_signals_barrier_;

  // Whether ConnectToSignal() is in progress.
  bool connect_signals_in_progress_ = false;

  // Whether the NotificationReplied signal could be connected to
  // and as such whether inline-reply support should be checked.
  bool connected_to_notification_replied_signal_ = false;

  // Calling CleanUp() while ConnectToSignal() is in progress leads to a crash.
  // This flag is used to defer the cleanup task until signals are connected.
  bool should_cleanup_on_signal_connected_ = false;

  scoped_refptr<base::RefCountedMemory> product_logo_png_bytes_;
  base::ScopedTempFile product_logo_file_;
  std::unique_ptr<base::FilePathWatcher> product_logo_file_watcher_;

  // A std::set<std::unique_ptr<T>> doesn't work well because
  // eg. std::set::erase(T) would require a std::unique_ptr<T>
  // argument, so the data would get double-destructed.
  template <typename T>
  using UnorderedUniqueSet = std::unordered_map<T*, std::unique_ptr<T>>;

  UnorderedUniqueSet<NotificationData> notifications_;

  bool clean_up_on_task_runner_called_ = false;
};

NotificationPlatformBridgeLinux::NotificationPlatformBridgeLinux()
    : NotificationPlatformBridgeLinux(nullptr) {}

NotificationPlatformBridgeLinux::NotificationPlatformBridgeLinux(
    scoped_refptr<dbus::Bus> bus)
    : impl_(new NotificationPlatformBridgeLinuxImpl(bus)) {
  impl_->Init();
}

NotificationPlatformBridgeLinux::~NotificationPlatformBridgeLinux() = default;

void NotificationPlatformBridgeLinux::Display(
    NotificationHandler::Type notification_type,
    Profile* profile,
    const message_center::Notification& notification,
    std::unique_ptr<NotificationCommon::Metadata> metadata) {
  impl_->Display(notification_type, profile, notification, std::move(metadata));
}

void NotificationPlatformBridgeLinux::Close(
    Profile* profile,
    const std::string& notification_id) {
  impl_->Close(profile, notification_id);
}

void NotificationPlatformBridgeLinux::GetDisplayed(
    Profile* profile,
    GetDisplayedNotificationsCallback callback) const {
  impl_->GetDisplayed(profile, std::move(callback));
}

void NotificationPlatformBridgeLinux::GetDisplayedForOrigin(
    Profile* profile,
    const GURL& origin,
    GetDisplayedNotificationsCallback callback) const {
  impl_->GetDisplayedForOrigin(profile, origin, std::move(callback));
}

void NotificationPlatformBridgeLinux::SetReadyCallback(
    NotificationBridgeReadyCallback callback) {
  impl_->SetReadyCallback(std::move(callback));
}

void NotificationPlatformBridgeLinux::DisplayServiceShutDown(Profile* profile) {
}

void NotificationPlatformBridgeLinux::CleanUp() {
  impl_->CleanUp();
}
