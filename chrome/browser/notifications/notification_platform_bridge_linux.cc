// Copyright 2017 The Chromium Authors. All rights reserved.
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
#include "base/files/file_path_watcher.h"
#include "base/files/file_util.h"
#include "base/i18n/number_formatting.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/ranges.h"
#include "base/strings/nullable_string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/dbus/dbus_thread_linux.h"
#include "chrome/browser/notifications/notification_display_service_impl.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/shell_integration_linux.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "net/base/escape.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "skia/ext/image_operations.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/message_center/public/cpp/notification.h"

namespace {

// DBus name / path.
const char kFreedesktopNotificationsName[] = "org.freedesktop.Notifications";
const char kFreedesktopNotificationsPath[] = "/org/freedesktop/Notifications";

// DBus methods.
const char kMethodCloseNotification[] = "CloseNotification";
const char kMethodGetCapabilities[] = "GetCapabilities";
const char kMethodNotify[] = "Notify";

// DBus signals.
const char kSignalActionInvoked[] = "ActionInvoked";
const char kSignalNotificationClosed[] = "NotificationClosed";

// Capabilities.
const char kCapabilityActionIcons[] = "action-icons";
const char kCapabilityActions[] = "actions";
const char kCapabilityBody[] = "body";
const char kCapabilityBodyHyperlinks[] = "body-hyperlinks";
const char kCapabilityBodyImages[] = "body-images";
const char kCapabilityBodyMarkup[] = "body-markup";
const char kCapabilityIconMulti[] = "icon-multi";
const char kCapabilityIconStatic[] = "icon-static";
const char kCapabilityPersistence[] = "persistence";
const char kCapabilitySound[] = "sound";

// Button IDs.
const char kCloseButtonId[] = "close";
const char kDefaultButtonId[] = "default";
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

base::string16 CreateNotificationTitle(
    const message_center::Notification& notification) {
  base::string16 title;
  if (notification.type() == message_center::NOTIFICATION_TYPE_PROGRESS) {
    title += base::FormatPercent(notification.progress());
    title += base::UTF8ToUTF16(" - ");
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
      NOTREACHED();
      FALLTHROUGH;
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
  width = base::ClampToRange<int>(scale * width, 1, kMaxImageWidth);
  height = base::ClampToRange<int>(scale * height, 1, kMaxImageHeight);
  return gfx::Image(
      gfx::ImageSkia::CreateFrom1xBitmap(skia::ImageOperations::Resize(
          *image_bitmap, skia::ImageOperations::RESIZE_LANCZOS3, width,
          height)));
}

bool ShouldAddCloseButton(const std::string& server_name) {
  // Cinnamon doesn't add a close button on notifications.  With eg. calendar
  // notifications, which are stay-on-screen, this can lead to a situation where
  // the only way to dismiss a notification is to click on it, which would
  // create an unwanted web navigation.  For this reason, manually add a close
  // button. (https://crbug.com/804637)
  return server_name == "cinnamon";
}

void ForwardNotificationOperationOnUiThread(
    NotificationCommon::Operation operation,
    NotificationHandler::Type notification_type,
    const GURL& origin,
    const std::string& notification_id,
    const base::Optional<int>& action_index,
    const base::Optional<bool>& by_user,
    const std::string& profile_id,
    bool is_incognito) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  g_browser_process->profile_manager()->LoadProfile(
      profile_id, is_incognito,
      base::Bind(&NotificationDisplayServiceImpl::ProfileLoadedCallback,
                 operation, notification_type, origin, notification_id,
                 action_index, base::nullopt /* reply */, by_user));
}

class ResourceFile {
 public:
  explicit ResourceFile(const base::FilePath& file_path)
      : file_path_(file_path) {
    DCHECK(!file_path.empty());
    DCHECK(file_path.IsAbsolute());
  }
  ~ResourceFile() { base::DeleteFile(file_path_, false); }

  const base::FilePath& file_path() const { return file_path_; }

 private:
  const base::FilePath file_path_;

  DISALLOW_COPY_AND_ASSIGN(ResourceFile);
};

// Writes |data| to a new temporary file and returns the ResourceFile
// that holds it.
std::unique_ptr<ResourceFile> WriteDataToTmpFile(
    const scoped_refptr<base::RefCountedMemory>& data) {
  int data_len = data->size();
  if (data_len == 0)
    return nullptr;
  base::FilePath file_path;
  if (!base::CreateTemporaryFile(&file_path))
    return nullptr;

  auto resource_file = std::make_unique<ResourceFile>(file_path);
  if (base::WriteFile(file_path, data->front_as<char>(), data_len) !=
      data_len) {
    resource_file.reset();
  }
  return resource_file;
}

}  // namespace

// static
NotificationPlatformBridge* NotificationPlatformBridge::Create() {
  return new NotificationPlatformBridgeLinux();
}

// static
bool NotificationPlatformBridge::CanHandleType(
    NotificationHandler::Type notification_type) {
  return notification_type != NotificationHandler::Type::TRANSIENT;
}

class NotificationPlatformBridgeLinuxImpl
    : public NotificationPlatformBridge,
      public content::NotificationObserver,
      public base::RefCountedThreadSafe<NotificationPlatformBridgeLinuxImpl> {
 public:
  explicit NotificationPlatformBridgeLinuxImpl(scoped_refptr<dbus::Bus> bus)
      : bus_(bus) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    task_runner_ = chrome::GetDBusTaskRunner();
    registrar_.Add(this, chrome::NOTIFICATION_APP_TERMINATING,
                   content::NotificationService::AllSources());
  }

  // InitOnTaskRunner() cannot be posted from within the constructor
  // because of a race condition.  The reference count for |this|
  // starts out as 0.  Posting the Init task would increment the count
  // to 1.  If the task finishes before the constructor returns, the
  // count will go to 0 and the object would be prematurely
  // destructed.
  void Init() {
    product_logo_png_bytes_ =
        gfx::Image(*ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
                       IDR_PRODUCT_LOGO_256))
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
        notification, body_images_supported_.value(),
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

  void SetReadyCallback(NotificationBridgeReadyCallback callback) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (connected_.has_value()) {
      std::move(callback).Run(connected_.value());
    } else {
      on_connected_callbacks_.push_back(std::move(callback));
    }
  }

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
    std::vector<std::unique_ptr<ResourceFile>> resource_files;
  };

  ~NotificationPlatformBridgeLinuxImpl() override {
    DCHECK(clean_up_on_task_runner_called_);
  }

  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    DCHECK_EQ(chrome::NOTIFICATION_APP_TERMINATING, type);
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

    notification_proxy_ =
        bus_->GetObjectProxy(kFreedesktopNotificationsName,
                             dbus::ObjectPath(kFreedesktopNotificationsPath));
    if (!notification_proxy_) {
      OnConnectionInitializationFinishedOnTaskRunner(
          ConnectionInitializationStatusCode::
              NATIVE_NOTIFICATIONS_NOT_SUPPORTED);
      return;
    }

    dbus::MethodCall get_capabilities_call(kFreedesktopNotificationsName,
                                           kMethodGetCapabilities);
    std::unique_ptr<dbus::Response> capabilities_response =
        notification_proxy_->CallMethodAndBlock(
            &get_capabilities_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT);
    if (capabilities_response) {
      dbus::MessageReader reader(capabilities_response.get());
      std::vector<std::string> capabilities;
      reader.PopArrayOfStrings(&capabilities);
      for (const std::string& capability : capabilities)
        capabilities_.insert(capability);
    }
    RecordMetricsForCapabilities();
    if (!base::ContainsKey(capabilities_, kCapabilityBody) ||
        !base::ContainsKey(capabilities_, kCapabilityActions)) {
      OnConnectionInitializationFinishedOnTaskRunner(
          ConnectionInitializationStatusCode::MISSING_REQUIRED_CAPABILITIES);
      return;
    }
    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(
            &NotificationPlatformBridgeLinuxImpl::SetBodyImagesSupported, this,
            base::ContainsKey(capabilities_, kCapabilityBodyImages)));

    dbus::MethodCall get_server_information_call(kFreedesktopNotificationsName,
                                                 "GetServerInformation");
    std::unique_ptr<dbus::Response> server_information_response =
        notification_proxy_->CallMethodAndBlock(
            &get_server_information_call,
            dbus::ObjectProxy::TIMEOUT_USE_DEFAULT);
    if (server_information_response) {
      dbus::MessageReader reader(server_information_response.get());
      reader.PopString(&server_name_);
    }

    connected_signals_barrier_ = base::BarrierClosure(
        2, base::Bind(&NotificationPlatformBridgeLinuxImpl::
                          OnConnectionInitializationFinishedOnTaskRunner,
                      this, ConnectionInitializationStatusCode::SUCCESS));
    notification_proxy_->ConnectToSignal(
        kFreedesktopNotificationsName, kSignalActionInvoked,
        base::Bind(&NotificationPlatformBridgeLinuxImpl::OnActionInvoked, this),
        base::Bind(&NotificationPlatformBridgeLinuxImpl::OnSignalConnected,
                   this));
    notification_proxy_->ConnectToSignal(
        kFreedesktopNotificationsName, kSignalNotificationClosed,
        base::Bind(&NotificationPlatformBridgeLinuxImpl::OnNotificationClosed,
                   this),
        base::Bind(&NotificationPlatformBridgeLinuxImpl::OnSignalConnected,
                   this));
  }

  void CleanUpOnTaskRunner() {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    if (bus_)
      bus_->ShutdownAndBlock();
    bus_ = nullptr;
    notification_proxy_ = nullptr;
    product_logo_png_bytes_ = nullptr;
    product_logo_file_.reset();
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
    writer.AppendString(
        product_logo_file_ ? "file://" + product_logo_file_->file_path().value()
                           : "");

    writer.AppendString(
        base::UTF16ToUTF8(CreateNotificationTitle(*notification)));

    std::ostringstream body;
    if (base::ContainsKey(capabilities_, kCapabilityBody)) {
      const bool body_markup =
          base::ContainsKey(capabilities_, kCapabilityBodyMarkup);

      if (notification->UseOriginAsContextMessage()) {
        std::string url_display_text =
            base::UTF16ToUTF8(url_formatter::FormatUrlForSecurityDisplay(
                notification->origin_url(),
                url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS));
        if (url_display_text.size() > kMaxAllowedOriginLength) {
          std::string domain_and_registry =
              net::registry_controlled_domains::GetDomainAndRegistry(
                  notification->origin_url(),
                  net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
          // localhost, raw IPs etc. are not handled by GetDomainAndRegistry.
          if (!domain_and_registry.empty()) {
            url_display_text = domain_and_registry;
          }
        }
        EscapeUnsafeCharacters(&url_display_text);
        if (body_markup &&
            base::ContainsKey(capabilities_, kCapabilityBodyHyperlinks)) {
          body << "<a href=\""
               << net::EscapeForHTML(notification->origin_url().spec()) << "\">"
               << url_display_text << "</a>\n\n";
        } else {
          body << url_display_text << "\n\n";
        }
      } else if (!notification->context_message().empty()) {
        std::string context =
            base::UTF16ToUTF8(notification->context_message());
        if (body_markup)
          EscapeUnsafeCharacters(&context);
        body << context << "\n\n";
      }

      std::string message = base::UTF16ToUTF8(notification->message());
      if (body_markup)
        EscapeUnsafeCharacters(&message);
      if (!message.empty())
        body << message << "\n";

      if (notification->type() == message_center::NOTIFICATION_TYPE_MULTIPLE) {
        for (const auto& item : notification->items()) {
          const std::string title = base::UTF16ToUTF8(item.title);
          const std::string message = base::UTF16ToUTF8(item.message);
          // TODO(peter): Figure out the right way to internationalize
          // this for RTL languages.
          if (body_markup)
            body << "<b>" << title << "</b> " << message << "\n";
          else
            body << title << " - " << message << "\n";
        }
      } else if (notification->type() ==
                     message_center::NOTIFICATION_TYPE_IMAGE &&
                 base::ContainsKey(capabilities_, kCapabilityBodyImages)) {
        std::unique_ptr<ResourceFile> image_file = WriteDataToTmpFile(
            ResizeImageToFdoMaxSize(notification->image()).As1xPNGBytes());
        if (image_file) {
          body << "<img src=\""
               << "file://" + net::EscapePath(image_file->file_path().value())
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
    if (base::ContainsKey(capabilities_, kCapabilityActions)) {
      data->action_start = data->action_end;
      for (const auto& button_info : notification->buttons()) {
        // FDO notification buttons can contain either an icon or a label,
        // but not both, and the type of all buttons must be the same (all
        // labels or all icons), so always use labels.
        const std::string id = base::NumberToString(data->action_end++);
        const std::string label = base::UTF16ToUTF8(button_info.title);
        actions.push_back(id);
        actions.push_back(label);
      }
      // Special case: the id "default" will not add a button, but
      // instead makes the entire notification clickable.
      actions.push_back(kDefaultButtonId);
      actions.push_back("Activate");
      // Always add a settings button for web notifications.
      if (notification_type != NotificationHandler::Type::EXTENSION) {
        actions.push_back(kSettingsButtonId);
        actions.push_back(
            l10n_util::GetStringUTF8(IDS_NOTIFICATION_BUTTON_SETTINGS));
      }
      if (ShouldAddCloseButton(server_name_)) {
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
        notification->never_timeout()
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
    base::FilePath desktop_file(
        shell_integration_linux::GetDesktopName(env.get()));
    const char kDesktopFileSuffix[] = ".desktop";
    DCHECK(base::EndsWith(desktop_file.value(), kDesktopFileSuffix,
                          base::CompareCase::SENSITIVE));
    desktop_file = desktop_file.RemoveFinalExtension();
    dbus::MessageWriter desktop_entry_writer(nullptr);
    hints_writer.OpenDictEntry(&desktop_entry_writer);
    desktop_entry_writer.AppendString("desktop-entry");
    desktop_entry_writer.AppendVariantOfString(desktop_file.value());
    hints_writer.CloseContainer(&desktop_entry_writer);

    std::unique_ptr<ResourceFile> icon_file =
        WriteDataToTmpFile(notification->icon().As1xPNGBytes());
    if (icon_file) {
      for (const std::string& hint_name : {"image_path", "image-path"}) {
        dbus::MessageWriter image_path_writer(nullptr);
        hints_writer.OpenDictEntry(&image_path_writer);
        image_path_writer.AppendString(hint_name);
        image_path_writer.AppendVariantOfString(icon_file->file_path().value());
        hints_writer.CloseContainer(&image_path_writer);
      }
      data->resource_files.push_back(std::move(icon_file));
    }

    writer.CloseContainer(&hints_writer);

    const int32_t kExpireTimeoutDefault = -1;
    const int32_t kExpireTimeoutNever = 0;
    writer.AppendInt32(
        notification->never_timeout()
            ? kExpireTimeoutNever
            : base::ContainsKey(capabilities_, kCapabilityPersistence)
                  ? kExpireTimeoutDefault
                  : kExpireTimeout);

    std::unique_ptr<dbus::Response> response =
        notification_proxy_->CallMethodAndBlock(
            &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT);
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
        notification_proxy_->CallMethodAndBlock(
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
    auto displayed = std::make_unique<std::set<std::string>>();
    for (const auto& pair : notifications_) {
      NotificationData* data = pair.first;
      if (data->profile_id == profile_id && data->is_incognito == incognito)
        displayed->insert(data->notification_id);
    }
    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::UI},
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

  void ForwardNotificationOperation(const base::Location& location,
                                    NotificationData* data,
                                    NotificationCommon::Operation operation,
                                    const base::Optional<int>& action_index,
                                    const base::Optional<bool>& by_user) {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    base::PostTaskWithTraits(
        location, {content::BrowserThread::UI},
        base::BindOnce(ForwardNotificationOperationOnUiThread, operation,
                       data->notification_type, data->origin_url,
                       data->notification_id, action_index, by_user,
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
          FROM_HERE, data, NotificationCommon::OPERATION_CLICK,
          base::nullopt /* action_index */, base::nullopt /* by_user */);
    } else if (action == kSettingsButtonId) {
      ForwardNotificationOperation(
          FROM_HERE, data, NotificationCommon::OPERATION_SETTINGS,
          base::nullopt /* action_index */, base::nullopt /* by_user */);
    } else if (action == kCloseButtonId) {
      CloseOnTaskRunner(data->profile_id, data->notification_id);
    } else {
      size_t id;
      if (!base::StringToSizeT(action, &id))
        return;
      size_t n_buttons = data->action_end - data->action_start;
      size_t id_zero_based = id - data->action_start;
      if (id_zero_based >= n_buttons)
        return;
      ForwardNotificationOperation(FROM_HERE, data,
                                   NotificationCommon::OPERATION_CLICK,
                                   id_zero_based, base::nullopt /* by_user */);
    }
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
    ForwardNotificationOperation(
        FROM_HERE, data, NotificationCommon::OPERATION_CLOSE,
        base::nullopt /* action_index */, true /* by_user */);
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
    UMA_HISTOGRAM_ENUMERATION(
        "Notifications.Linux.BridgeInitializationStatus",
        static_cast<int>(status),
        static_cast<int>(ConnectionInitializationStatusCode::NUM_ITEMS));
    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(&NotificationPlatformBridgeLinuxImpl::
                           OnConnectionInitializationFinishedOnUiThread,
                       this,
                       status == ConnectionInitializationStatusCode::SUCCESS));
  }

  void OnSignalConnected(const std::string& interface_name,
                         const std::string& signal_name,
                         bool success) {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    if (!success) {
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
    product_logo_file_.reset();
    product_logo_file_watcher_.reset();
  }

  void RecordMetricsForCapabilities() {
    // Histogram macros must be called with the same name for each
    // callsite, so we can't roll the below into a nice loop.
    UMA_HISTOGRAM_BOOLEAN(
        "Notifications.Freedesktop.Capabilities.ActionIcons",
        base::ContainsKey(capabilities_, kCapabilityActionIcons));
    UMA_HISTOGRAM_BOOLEAN("Notifications.Freedesktop.Capabilities.Actions",
                          base::ContainsKey(capabilities_, kCapabilityActions));
    UMA_HISTOGRAM_BOOLEAN("Notifications.Freedesktop.Capabilities.Body",
                          base::ContainsKey(capabilities_, kCapabilityBody));
    UMA_HISTOGRAM_BOOLEAN(
        "Notifications.Freedesktop.Capabilities.BodyHyperlinks",
        base::ContainsKey(capabilities_, kCapabilityBodyHyperlinks));
    UMA_HISTOGRAM_BOOLEAN(
        "Notifications.Freedesktop.Capabilities.BodyImages",
        base::ContainsKey(capabilities_, kCapabilityBodyImages));
    UMA_HISTOGRAM_BOOLEAN(
        "Notifications.Freedesktop.Capabilities.BodyMarkup",
        base::ContainsKey(capabilities_, kCapabilityBodyMarkup));
    UMA_HISTOGRAM_BOOLEAN(
        "Notifications.Freedesktop.Capabilities.IconMulti",
        base::ContainsKey(capabilities_, kCapabilityIconMulti));
    UMA_HISTOGRAM_BOOLEAN(
        "Notifications.Freedesktop.Capabilities.IconStatic",
        base::ContainsKey(capabilities_, kCapabilityIconStatic));
    UMA_HISTOGRAM_BOOLEAN(
        "Notifications.Freedesktop.Capabilities.Persistence",
        base::ContainsKey(capabilities_, kCapabilityPersistence));
    UMA_HISTOGRAM_BOOLEAN("Notifications.Freedesktop.Capabilities.Sound",
                          base::ContainsKey(capabilities_, kCapabilitySound));
  }

  void RewriteProductLogoFile() {
    product_logo_file_watcher_.reset();
    product_logo_file_ = WriteDataToTmpFile(product_logo_png_bytes_);
    if (!product_logo_file_)
      return;
    // Temporary files may periodically get cleaned up on Linux.
    // Watch for file deletion and rewrite the file in case we have a
    // long-running Chrome process.
    product_logo_file_watcher_ = std::make_unique<base::FilePathWatcher>();
    if (!product_logo_file_watcher_->Watch(
            product_logo_file_->file_path(), false,
            base::Bind(
                &NotificationPlatformBridgeLinuxImpl::OnProductLogoFileChanged,
                this))) {
      product_logo_file_.reset();
      product_logo_file_watcher_.reset();
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  // Members used only on the UI thread.

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  content::NotificationRegistrar registrar_;

  // State necessary for OnConnectionInitializationFinished() and
  // SetReadyCallback().
  base::Optional<bool> connected_;
  std::vector<NotificationBridgeReadyCallback> on_connected_callbacks_;

  // Notification servers very rarely have the 'body-images'
  // capability, so try to avoid an image copy if possible.
  base::Optional<bool> body_images_supported_;

  //////////////////////////////////////////////////////////////////////////////
  // Members used only on the task runner thread.

  scoped_refptr<dbus::Bus> bus_;

  dbus::ObjectProxy* notification_proxy_ = nullptr;

  std::unordered_set<std::string> capabilities_;

  std::string server_name_;

  base::Closure connected_signals_barrier_;

  scoped_refptr<base::RefCountedMemory> product_logo_png_bytes_;
  std::unique_ptr<ResourceFile> product_logo_file_;
  std::unique_ptr<base::FilePathWatcher> product_logo_file_watcher_;

  // A std::set<std::unique_ptr<T>> doesn't work well because
  // eg. std::set::erase(T) would require a std::unique_ptr<T>
  // argument, so the data would get double-destructed.
  template <typename T>
  using UnorderedUniqueSet = std::unordered_map<T*, std::unique_ptr<T>>;

  UnorderedUniqueSet<NotificationData> notifications_;

  bool clean_up_on_task_runner_called_ = false;

  DISALLOW_COPY_AND_ASSIGN(NotificationPlatformBridgeLinuxImpl);
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

void NotificationPlatformBridgeLinux::SetReadyCallback(
    NotificationBridgeReadyCallback callback) {
  impl_->SetReadyCallback(std::move(callback));
}

void NotificationPlatformBridgeLinux::CleanUp() {
  impl_->CleanUp();
}
