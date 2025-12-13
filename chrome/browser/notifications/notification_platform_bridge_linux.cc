// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_platform_bridge_linux.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/callback_list.h"
#include "base/check_deref.h"
#include "base/containers/contains.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/number_formatting.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/nix/xdg_util.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "base/types/expected.h"
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
#include "components/dbus/utils/call_method.h"
#include "components/dbus/utils/check_for_service_and_start.h"
#include "components/dbus/utils/connect_to_signal.h"
#include "components/dbus/utils/read_value.h"
#include "components/dbus/utils/variant.h"
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
const char kMethodNotify[] = "Notify";

// DBus signals.
const char kSignalActivationToken[] = "ActivationToken";
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

struct NotificationTempFiles {
  base::FilePath dir_path;
  base::SequenceBound<base::ScopedTempDir> dir;
  bool has_logo = false;
  bool has_icon = false;
  bool has_image = false;
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

uint8_t NotificationPriorityToFdoUrgency(int priority) {
  switch (priority) {
    case message_center::MIN_PRIORITY:
    case message_center::LOW_PRIORITY:
      return URGENCY_LOW;
    case message_center::HIGH_PRIORITY:
    case message_center::MAX_PRIORITY:
      return URGENCY_CRITICAL;
    case message_center::DEFAULT_PRIORITY:
      return URGENCY_NORMAL;
    default:
      NOTREACHED();
  }
}

// Constrain |image|'s size to |kMaxImageWidth|x|kMaxImageHeight|. If
// the image does not need to be resized, or the image is empty,
// returns |image| directly.
gfx::Image ResizeImageToFdoMaxSize(const gfx::Image& image) {
  if (image.IsEmpty()) {
    return image;
  }
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

void ForwardNotificationOperation(NotificationOperation operation,
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
                     action_index, reply, by_user, /*is_suspicious=*/false,
                     base::DoNothing()));
}

bool WriteImageFile(scoped_refptr<base::RefCountedMemory> image,
                    const base::FilePath& file_path) {
  if (!image || !image->size()) {
    return false;
  }
  return base::WriteFile(file_path, *image.get());
}

// Must be called on an IO task runner.
NotificationTempFiles WriteNotificationResourceFiles(
    scoped_refptr<base::RefCountedMemory> logo,
    scoped_refptr<base::RefCountedMemory> icon,
    scoped_refptr<base::RefCountedMemory> image) {
  NotificationTempFiles result;
  base::ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDir()) {
    return result;
  }

  const base::FilePath dir_path = temp_dir.GetPath();

  result.has_logo = WriteImageFile(logo, dir_path.Append("logo.png"));
  result.has_icon = WriteImageFile(icon, dir_path.Append("icon.png"));
  result.has_image = WriteImageFile(image, dir_path.Append("image.png"));

  result.dir_path = dir_path;
  result.dir = base::SequenceBound<base::ScopedTempDir>(
      base::SequencedTaskRunner::GetCurrentDefault(), std::move(temp_dir));
  return result;
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

class NotificationPlatformBridgeLinuxImpl : public NotificationPlatformBridge {
 public:
  explicit NotificationPlatformBridgeLinuxImpl(scoped_refptr<dbus::Bus> bus)
      : file_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::USER_VISIBLE})),
        bus_(std::move(bus)) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    CHECK(bus_);

    on_app_terminating_subscription_ =
        browser_shutdown::AddAppTerminatingCallback(base::BindOnce(
            &NotificationPlatformBridgeLinuxImpl::OnAppTerminating,
            weak_factory_.GetWeakPtr()));
  }

  NotificationPlatformBridgeLinuxImpl(
      const NotificationPlatformBridgeLinuxImpl&) = delete;
  NotificationPlatformBridgeLinuxImpl& operator=(
      const NotificationPlatformBridgeLinuxImpl&) = delete;

  ~NotificationPlatformBridgeLinuxImpl() override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    CleanUp();
  }

  // Sets up the D-Bus connection.
  void Init() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    dbus_utils::CheckForServiceAndStart(
        bus_, kFreedesktopNotificationsName,
        base::BindOnce(&NotificationPlatformBridgeLinuxImpl::OnServiceStarted,
                       weak_factory_.GetWeakPtr()));
  }

  // Makes the "Notify" call to D-Bus.
  void Display(
      NotificationHandler::Type notification_type,
      Profile* profile,
      const message_center::Notification& notification,
      std::unique_ptr<NotificationCommon::Metadata> metadata) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    std::string profile_id = GetProfileId(profile);
    bool is_incognito = profile->IsOffTheRecord();
    auto copy_notification =
        std::make_unique<message_center::Notification>(notification);
    NotificationData* data =
        FindNotificationData(copy_notification->id(), profile_id, is_incognito);
    if (data) {
      // Update an existing notification.
      data->notification_type = notification_type;
    } else {
      // Send the notification for the first time.
      data = new NotificationData(notification_type, copy_notification->id(),
                                  profile_id, is_incognito,
                                  copy_notification->origin_url());
      notifications_.emplace(data, base::WrapUnique(data));
    }

    // Prepare resource files.
    gfx::Image product_logo(
        *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            IDR_PRODUCT_LOGO_64));

    gfx::Image notification_icon = ResizeImageToFdoMaxSize(
        gfx::Image(copy_notification->icon().Rasterize(nullptr)));

    gfx::Image notification_image;
    if (copy_notification->type() == message_center::NOTIFICATION_TYPE_IMAGE &&
        base::Contains(capabilities_, kCapabilityBodyImages)) {
      notification_image = ResizeImageToFdoMaxSize(copy_notification->image());
    }

    file_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&WriteNotificationResourceFiles,
                       product_logo.As1xPNGBytes(),
                       notification_icon.As1xPNGBytes(),
                       notification_image.As1xPNGBytes()),
        base::BindOnce(
            &NotificationPlatformBridgeLinuxImpl::OnFilesWrittenForDisplay,
            weak_factory_.GetWeakPtr(), notification_type, profile_id,
            is_incognito, std::move(copy_notification), data->dbus_id));
  }

  void Close(Profile* profile, const std::string& notification_id) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    CloseImpl(GetProfileId(profile), notification_id);
  }

  void GetDisplayed(Profile* profile,
                    GetDisplayedNotificationsCallback callback) const override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    std::set<std::string> displayed;
    for (const auto& pair : notifications_) {
      NotificationData* data = pair.first;
      if (data->profile_id == GetProfileId(profile) &&
          data->is_incognito == profile->IsOffTheRecord()) {
        displayed.insert(data->notification_id);
      }
    }
    std::move(callback).Run(std::move(displayed), true);
  }

  void GetDisplayedForOrigin(
      Profile* profile,
      const GURL& origin,
      GetDisplayedNotificationsCallback callback) const override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    std::set<std::string> displayed;
    for (const auto& pair : notifications_) {
      NotificationData* data = pair.first;
      if (data->profile_id == GetProfileId(profile) &&
          data->is_incognito == profile->IsOffTheRecord() &&
          url::IsSameOriginWith(data->origin_url, origin)) {
        displayed.insert(data->notification_id);
      }
    }
    std::move(callback).Run(std::move(displayed), true);
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
    notification_proxy_ = nullptr;
    bus_.reset();
    notifications_.clear();
    weak_factory_.InvalidateWeakPtrs();
  }

 private:
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
    NotificationTempFiles files;
  };

  void OnAppTerminating() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    // The browser process is about to exit.  Run CleanUp() while we still can.
    CleanUp();
  }

  void OnServiceStarted(std::optional<bool> service_started) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (!service_started.value_or(false)) {
      OnConnectionInitializationFinished(
          ConnectionInitializationStatusCode::
              NATIVE_NOTIFICATIONS_NOT_SUPPORTED);
      return;
    }
    notification_proxy_ =
        bus_->GetObjectProxy(kFreedesktopNotificationsName,
                             dbus::ObjectPath(kFreedesktopNotificationsPath));

    dbus_utils::CallMethod<"", "as">(
        notification_proxy_, kFreedesktopNotificationsName,
        kMethodGetCapabilities,
        base::BindOnce(
            &NotificationPlatformBridgeLinuxImpl::OnGetCapabilitiesResponse,
            weak_factory_.GetWeakPtr()));
  }

  void OnGetCapabilitiesResponse(
      dbus_utils::CallMethodResult<std::vector<std::string>> result) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (!result.has_value()) {
      OnConnectionInitializationFinished(
          ConnectionInitializationStatusCode::MISSING_REQUIRED_CAPABILITIES);
      return;
    }

    auto& [capabilities] = result.value();
    for (auto& item : capabilities) {
      capabilities_.insert(std::move(item));
    }
    if (!base::Contains(capabilities_, kCapabilityBody) ||
        !base::Contains(capabilities_, kCapabilityActions)) {
      OnConnectionInitializationFinished(
          ConnectionInitializationStatusCode::MISSING_REQUIRED_CAPABILITIES);
      return;
    }
    body_images_supported_ =
        base::Contains(capabilities_, kCapabilityBodyImages);

    dbus_utils::CallMethod<"", "ssss">(
        notification_proxy_, kFreedesktopNotificationsName,
        "GetServerInformation",
        base::BindOnce(
            &NotificationPlatformBridgeLinuxImpl::OnGetServerInfoResponse,
            weak_factory_.GetWeakPtr()));
  }

  void OnGetServerInfoResponse(
      dbus_utils::
          CallMethodResult<std::string, std::string, std::string, std::string>
              result) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (result.has_value()) {
      auto& [server_name, vendor, server_version, spec_version] =
          result.value();
      server_name_ = std::move(server_name);
      server_version_ = base::Version(std::move(server_version));
    }

    connected_signals_barrier_ = base::BarrierClosure(
        4, base::BindOnce(&NotificationPlatformBridgeLinuxImpl::
                              OnConnectionInitializationFinished,
                          weak_factory_.GetWeakPtr(),
                          ConnectionInitializationStatusCode::SUCCESS));

    dbus_utils::ConnectToSignal<"us">(
        notification_proxy_, kFreedesktopNotificationsName,
        kSignalActivationToken,
        base::BindRepeating(
            &NotificationPlatformBridgeLinuxImpl::OnActivationToken,
            weak_factory_.GetWeakPtr()),
        base::BindOnce(&NotificationPlatformBridgeLinuxImpl::OnSignalConnected,
                       weak_factory_.GetWeakPtr()));

    dbus_utils::ConnectToSignal<"us">(
        notification_proxy_, kFreedesktopNotificationsName,
        kSignalActionInvoked,
        base::BindRepeating(
            &NotificationPlatformBridgeLinuxImpl::OnActionInvoked,
            weak_factory_.GetWeakPtr()),
        base::BindOnce(&NotificationPlatformBridgeLinuxImpl::OnSignalConnected,
                       weak_factory_.GetWeakPtr()));

    dbus_utils::ConnectToSignal<"uu">(
        notification_proxy_, kFreedesktopNotificationsName,
        kSignalNotificationClosed,
        base::BindRepeating(
            &NotificationPlatformBridgeLinuxImpl::OnNotificationClosed,
            weak_factory_.GetWeakPtr()),
        base::BindOnce(&NotificationPlatformBridgeLinuxImpl::OnSignalConnected,
                       weak_factory_.GetWeakPtr()));

    dbus_utils::ConnectToSignal<"us">(
        notification_proxy_, kFreedesktopNotificationsName,
        kSignalNotificationReplied,
        base::BindRepeating(
            &NotificationPlatformBridgeLinuxImpl::OnNotificationReplied,
            weak_factory_.GetWeakPtr()),
        base::BindOnce(&NotificationPlatformBridgeLinuxImpl::OnSignalConnected,
                       weak_factory_.GetWeakPtr()));
  }
  void OnFilesWrittenForDisplay(
      NotificationHandler::Type notification_type,
      const std::string& profile_id,
      bool is_incognito,
      std::unique_ptr<message_center::Notification> notification,
      uint32_t dbus_id,
      NotificationTempFiles files) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    NotificationData* data =
        FindNotificationData(notification->id(), profile_id, is_incognito);
    if (!data) {
      return;
    }

    data->files = std::move(files);

    std::string app_name(l10n_util::GetStringUTF8(IDS_PRODUCT_NAME));

    std::string app_icon(
        data->files.has_logo
            ? "file://" + data->files.dir_path.Append("logo.png").value()
            : "");

    std::string summary(
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
      if (body_markup) {
        EscapeUnsafeCharacters(&message);
      }
      if (!message.empty()) {
        body << message << "\n";
      }
      if (notification->type() == message_center::NOTIFICATION_TYPE_MULTIPLE) {
        for (const auto& item : notification->items()) {
          const std::string item_title = base::UTF16ToUTF8(item.title());
          const std::string item_message = base::UTF16ToUTF8(item.message());
          // TODO(peter): Figure out the right way to internationalize
          // this for RTL languages.
          if (body_markup) {
            body << "<b>" << item_title << "</b> " << item_message << "\n";
          } else {
            body << item_title << " - " << item_message << "\n";
          }
        }
      } else if (notification->type() ==
                     message_center::NOTIFICATION_TYPE_IMAGE &&
                 data->files.has_image &&
                 base::Contains(capabilities_, kCapabilityBodyImages)) {
        body << "<img src=\"file://"
             << base::EscapePath(
                    data->files.dir_path.Append("image.png").value())
             << "\" alt=\"\"/>\n";
      }
    }
    std::string body_str = body.str();
    base::TrimString(body_str, "\n", &body_str);

    // Even-indexed elements in this vector are action IDs passed back to
    // us in OnActionInvoked().  Odd-indexed ones contain the button text.
    std::vector<std::string> actions;
    std::optional<std::u16string> inline_reply_placeholder;
    if (base::Contains(capabilities_, kCapabilityActions)) {
      const bool has_support_for_inline_reply =
          connected_to_notification_replied_signal_ &&
          base::Contains(capabilities_, kCapabilityInlineReply);
      data->action_start = data->action_end;

      for (const auto& button_info : notification->buttons()) {
        const std::string label = base::UTF16ToUTF8(button_info.title);
        if (has_support_for_inline_reply && button_info.placeholder) {
          // There can only be one inline-reply action
          if (inline_reply_placeholder) {
            continue;
          }

          actions.emplace_back(kInlineReplyButtonId);
          actions.emplace_back(label);

          inline_reply_placeholder = button_info.placeholder;
          continue;
        }
        // FDO notification buttons can contain either an icon or a label,
        // but not both, and the type of all buttons must be the same (all
        // labels or all icons), so always use labels.
        const std::string id = base::NumberToString(data->action_end++);
        actions.emplace_back(id);
        actions.emplace_back(label);
      }
      // Special case: the id "default" will not add a button, but
      // instead makes the entire notification clickable.
      actions.emplace_back(kDefaultButtonId);
      actions.emplace_back("Activate");
      // Always add a settings button for web notifications.
      if (notification->should_show_settings_button()) {
        actions.emplace_back(kSettingsButtonId);
        actions.emplace_back(
            l10n_util::GetStringUTF8(IDS_NOTIFICATION_BUTTON_SETTINGS));
      }
      if (ShouldAddCloseButton(server_name_, server_version_)) {
        actions.emplace_back(kCloseButtonId);
        actions.emplace_back(
            l10n_util::GetStringUTF8(IDS_NOTIFICATION_BUTTON_CLOSE));
      }
    }

    std::map<std::string, dbus_utils::Variant> hints;

    uint8_t urgency =
        notification->never_timeout() &&
                ShouldMarkPersistentNotificationsAsCritical(server_name_)
            ? URGENCY_CRITICAL
            : NotificationPriorityToFdoUrgency(notification->priority());
    hints.emplace("urgency", dbus_utils::Variant::Wrap<"y">(urgency));

    if (notification->silent()) {
      hints.emplace("suppress-sound", dbus_utils::Variant::Wrap<"b">(true));
    }

    std::unique_ptr<base::Environment> env = base::Environment::Create();
    base::FilePath desktop_file(chrome::GetDesktopName(env.get()));
    static const char kDesktopFileSuffix[] = ".desktop";
    DCHECK(base::EndsWith(desktop_file.value(), kDesktopFileSuffix,
                          base::CompareCase::SENSITIVE));
    desktop_file = desktop_file.RemoveFinalExtension();
    hints.emplace("desktop-entry",
                  dbus_utils::Variant::Wrap<"s">(desktop_file.value()));

    if (data->files.has_icon) {
      const base::FilePath icon_path = data->files.dir_path.Append("icon.png");
      hints.emplace("image_path",
                    dbus_utils::Variant::Wrap<"s">(icon_path.value()));
      hints.emplace("image-path",
                    dbus_utils::Variant::Wrap<"s">(icon_path.value()));
    }

    if (has_support_for_kde_origin_name && !context_display_text.empty()) {
      hints.emplace(
          kCapabilityXKdeOriginName,
          dbus_utils::Variant::Wrap<"s">(std::move(context_display_text)));
    }

    if (inline_reply_placeholder.has_value()) {
      hints.emplace(kCapabilityXKdeReplyPlaceholderText,
                    dbus_utils::Variant::Wrap<"s">(
                        base::UTF16ToUTF8(inline_reply_placeholder.value())));
    }

    const int32_t kExpireTimeoutDefault = -1;
    const int32_t kExpireTimeoutNever = 0;
    int32_t expire_timeout =
        notification->never_timeout() ? kExpireTimeoutNever
        : base::Contains(capabilities_, kCapabilityPersistence)
            ? kExpireTimeoutDefault
            : kExpireTimeout;

    dbus_utils::CallMethod<"susssasa{sv}i", "u">(
        notification_proxy_, kFreedesktopNotificationsName, kMethodNotify,
        base::BindOnce(&NotificationPlatformBridgeLinuxImpl::OnNotifyResponse,
                       weak_factory_.GetWeakPtr(), notification->id(),
                       profile_id, is_incognito),
        std::move(app_name), dbus_id, std::move(app_icon), std::move(summary),
        std::move(body_str), std::move(actions), std::move(hints),
        expire_timeout);
  }

  void OnNotifyResponse(const std::string& notification_id,
                        const std::string& profile_id,
                        bool is_incognito,
                        dbus_utils::CallMethodResult<uint32_t> result) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    NotificationData* data =
        FindNotificationData(notification_id, profile_id, is_incognito);
    if (!data) {
      return;
    }
    data->dbus_id = 0;
    if (result.has_value()) {
      auto& [dbus_id] = result.value();
      data->dbus_id = dbus_id;
    }
    if (!data->dbus_id) {
      // There was some sort of error with creating the notification.
      notifications_.erase(data);
    }
  }

  // Makes the "CloseNotification" call to D-Bus.
  void CloseImpl(const std::string& profile_id,
                 const std::string& notification_id) {
    std::vector<NotificationData*> to_erase;
    for (const auto& pair : notifications_) {
      NotificationData* data = pair.first;
      if (data->notification_id == notification_id &&
          data->profile_id == profile_id) {
        dbus_utils::CallMethod<"u", "">(
            notification_proxy_, kFreedesktopNotificationsName,
            kMethodCloseNotification,
            base::BindOnce([](dbus_utils::CallMethodResult<>) {}),
            data->dbus_id);
        to_erase.push_back(data);
      }
    }
    for (NotificationData* data : to_erase) {
      notifications_.erase(data);
    }
  }

  NotificationData* FindNotificationData(const std::string& notification_id,
                                         const std::string& profile_id,
                                         bool is_incognito) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
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
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (!dbus_id) {
      return nullptr;
    }
    for (const auto& pair : notifications_) {
      NotificationData* data = pair.first;
      if (data->dbus_id == dbus_id) {
        return data;
      }
    }
    return nullptr;
  }

  void OnActivationToken(dbus_utils::ConnectToSignalResultSig<"us"> result) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (!result.has_value()) {
      LOG(ERROR) << "Error parsing ActivationToken signal";
      return;
    }
    auto& [dbus_id, token] = result.value();
    base::nix::SetActivationToken(token);
  }

  void OnActionInvoked(dbus_utils::ConnectToSignalResultSig<"us"> result) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (!result.has_value()) {
      LOG(ERROR) << "Error parsing ActionInvoked signal";
      return;
    }
    auto& [dbus_id, dbus_action] = result.value();
    NotificationData* data = FindNotificationDataWithDBusId(dbus_id);
    if (!data) {
      return;
    }

    const std::string& action = dbus_action;
    if (action == kDefaultButtonId) {
      ForwardNotificationOperation(
          NotificationOperation::kClick, data->notification_type,
          data->origin_url, data->notification_id,
          /*action_index=*/std::nullopt, /*by_user=*/std::nullopt,
          /*reply=*/std::nullopt, data->profile_id, data->is_incognito);
    } else if (action == kSettingsButtonId) {
      ForwardNotificationOperation(
          NotificationOperation::kSettings, data->notification_type,
          data->origin_url, data->notification_id,
          /*action_index=*/std::nullopt, /*by_user=*/std::nullopt,
          /*reply=*/std::nullopt, data->profile_id, data->is_incognito);
    } else if (action == kCloseButtonId) {
      ForwardNotificationOperation(
          NotificationOperation::kClose, data->notification_type,
          data->origin_url, data->notification_id,
          /*action_index=*/std::nullopt, /*by_user=*/true,
          /*reply=*/std::nullopt, data->profile_id, data->is_incognito);
      CloseImpl(data->profile_id, data->notification_id);
    } else {
      size_t id;
      if (!base::StringToSizeT(action, &id)) {
        return;
      }
      size_t n_buttons = data->action_end - data->action_start;
      size_t id_zero_based = id - data->action_start;
      if (id_zero_based >= n_buttons) {
        return;
      }
      ForwardNotificationOperation(
          NotificationOperation::kClick, data->notification_type,
          data->origin_url, data->notification_id, id_zero_based,
          /*by_user=*/std::nullopt,
          /*reply=*/std::nullopt, data->profile_id, data->is_incognito);
    }
  }

  void OnNotificationReplied(
      dbus_utils::ConnectToSignalResultSig<"us"> result) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (!result.has_value()) {
      LOG(ERROR) << "Error parsing NotificationReplied signal";
      return;
    }
    auto& [dbus_id, reply] = result.value();
    NotificationData* data = FindNotificationDataWithDBusId(dbus_id);
    if (!data) {
      return;
    }

    ForwardNotificationOperation(
        NotificationOperation::kClick, data->notification_type,
        data->origin_url, data->notification_id, /*action_index=*/std::nullopt,
        /*by_user=*/std::nullopt, base::UTF8ToUTF16(reply), data->profile_id,
        data->is_incognito);
  }

  void OnNotificationClosed(dbus_utils::ConnectToSignalResultSig<"uu"> result) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (!result.has_value()) {
      LOG(ERROR) << "Error parsing NotificationClosed signal";
      return;
    }
    auto& [dbus_id, reason] = result.value();
    NotificationData* data = FindNotificationDataWithDBusId(dbus_id);
    if (!data) {
      return;
    }

    // TODO(peter): Can we support `by_user` appropriately here?
    ForwardNotificationOperation(
        NotificationOperation::kClose, data->notification_type,
        data->origin_url, data->notification_id, std::nullopt, true,
        std::nullopt, data->profile_id, data->is_incognito);

    notifications_.erase(data);
  }

  // Called once the connection has been set up (or not).
  void OnConnectionInitializationFinished(
      ConnectionInitializationStatusCode status) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    bool success = (status == ConnectionInitializationStatusCode::SUCCESS);
    connected_ = success;
    for (auto& callback : on_connected_callbacks_) {
      std::move(callback).Run(success);
    }
    on_connected_callbacks_.clear();
    if (!success) {
      CleanUp();
    }
  }

  void OnSignalConnected(const std::string& interface_name,
                         const std::string& signal_name,
                         bool success) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    bool isNotificationRepliedSignal =
        (signal_name == kSignalNotificationReplied);
    if (isNotificationRepliedSignal) {
      connected_to_notification_replied_signal_ = success;
    } else if (!success) {
      OnConnectionInitializationFinished(
          ConnectionInitializationStatusCode::COULD_NOT_CONNECT_TO_SIGNALS);
      return;
    }
    connected_signals_barrier_.Run();
  }

  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  base::CallbackListSubscription on_app_terminating_subscription_;

  // State necessary for OnConnectionInitializationFinished() and
  // SetReadyCallback().
  std::optional<bool> connected_;
  std::vector<NotificationBridgeReadyCallback> on_connected_callbacks_;

  // Notification servers very rarely have the 'body-images'
  // capability, so try to avoid an image copy if possible.
  std::optional<bool> body_images_supported_;

  scoped_refptr<dbus::Bus> bus_;
  raw_ptr<dbus::ObjectProxy> notification_proxy_ = nullptr;

  std::unordered_set<std::string> capabilities_;

  std::string server_name_;
  base::Version server_version_;

  base::RepeatingClosure connected_signals_barrier_;

  // Whether the NotificationReplied signal could be connected to
  // and as such whether inline-reply support should be checked.
  bool connected_to_notification_replied_signal_ = false;

  // A std::set<std::unique_ptr<T>> doesn't work well because
  // eg. std::set::erase(T) would require a std::unique_ptr<T>
  // argument, so the data would get double-destructed.
  template <typename T>
  using UnorderedUniqueSet = std::unordered_map<T*, std::unique_ptr<T>>;

  UnorderedUniqueSet<NotificationData> notifications_;

  base::WeakPtrFactory<NotificationPlatformBridgeLinuxImpl> weak_factory_{this};
};

NotificationPlatformBridgeLinux::NotificationPlatformBridgeLinux()
    : NotificationPlatformBridgeLinux(
          dbus_thread_linux::GetSharedSessionBus()) {}

NotificationPlatformBridgeLinux::NotificationPlatformBridgeLinux(
    scoped_refptr<dbus::Bus> bus)
    : impl_(std::make_unique<NotificationPlatformBridgeLinuxImpl>(bus)) {
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
