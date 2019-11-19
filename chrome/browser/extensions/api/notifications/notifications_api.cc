// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/notifications/notifications_api.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/guid.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/notifications/extension_notification_display_helper.h"
#include "chrome/browser/extensions/api/notifications/extension_notification_display_helper_factory.h"
#include "chrome/browser/extensions/api/notifications/extension_notification_handler.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/notifications/notifier_state_tracker.h"
#include "chrome/browser/notifications/notifier_state_tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/notifications/notification_style.h"
#include "components/keyed_service/content/browser_context_keyed_service_shutdown_notifier_factory.h"
#include "components/keyed_service/core/keyed_service_shutdown_notifier.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/extension.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/layout.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/skia_util.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "url/gurl.h"

using message_center::NotifierId;

namespace extensions {

namespace notifications = api::notifications;

namespace {

// The maximum length of a notification ID, in number of characters. Some
// platforms have limitattions on the length of the ID.
constexpr int kNotificationIdLengthLimit = 500;

const char kMissingRequiredPropertiesForCreateNotification[] =
    "Some of the required properties are missing: type, iconUrl, title and "
    "message.";
const char kUnableToDecodeIconError[] =
    "Unable to successfully use the provided image.";
const char kUnexpectedProgressValueForNonProgressType[] =
    "The progress value should not be specified for non-progress notification";
const char kInvalidProgressValue[] =
    "The progress value should range from 0 to 100";
const char kExtraListItemsProvided[] =
    "List items provided for notification type != list";
const char kExtraImageProvided[] =
    "Image resource provided for notification type != image";
const char kNotificationIdTooLong[] =
    "The notification's ID should be %d characters or less";

#if !defined(OS_CHROMEOS)
const char kLowPriorityDeprecatedOnPlatform[] =
    "Low-priority notifications are deprecated on this platform.";
#endif

// Given an extension id and another id, returns an id that is unique
// relative to other extensions.
std::string CreateScopedIdentifier(const std::string& extension_id,
                                   const std::string& id) {
  return extension_id + "-" + id;
}

// Removes the unique internal identifier to send the ID as the
// extension expects it.
std::string StripScopeFromIdentifier(const std::string& extension_id,
                                     const std::string& scoped_id) {
  size_t index_of_separator = extension_id.length() + 1;
  DCHECK_LT(index_of_separator, scoped_id.length());

  return scoped_id.substr(index_of_separator);
}

const gfx::ImageSkia CreateSolidColorImage(int width,
                                           int height,
                                           SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(color);
  return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
}

// Take the alpha channel of small_image, mask it with the foreground,
// then add the masked foreground on top of the background
const gfx::Image GetMaskedSmallImage(const gfx::ImageSkia& small_image) {
  int width = small_image.width();
  int height = small_image.height();

  // Background color grey
  const gfx::ImageSkia background = CreateSolidColorImage(
      width, height, message_center::kSmallImageMaskBackgroundColor);
  // Foreground color white
  const gfx::ImageSkia foreground = CreateSolidColorImage(
      width, height, message_center::kSmallImageMaskForegroundColor);
  const gfx::ImageSkia masked_small_image =
      gfx::ImageSkiaOperations::CreateMaskedImage(foreground, small_image);
  return gfx::Image(gfx::ImageSkiaOperations::CreateSuperimposedImage(
      background, masked_small_image));
}

// Converts the |notification_bitmap| (in RGBA format) to the |*return_image|
// (which is in ARGB format).
bool NotificationBitmapToGfxImage(
    float max_scale,
    const gfx::Size& target_size_dips,
    const notifications::NotificationBitmap& notification_bitmap,
    gfx::Image* return_image) {
  const int max_device_pixel_width = target_size_dips.width() * max_scale;
  const int max_device_pixel_height = target_size_dips.height() * max_scale;

  const int kBytesPerPixel = 4;

  const int width = notification_bitmap.width;
  const int height = notification_bitmap.height;

  if (width < 0 || height < 0 || width > max_device_pixel_width ||
      height > max_device_pixel_height)
    return false;

  // Ensure we have rgba data.
  std::vector<uint8_t>* rgba_data = notification_bitmap.data.get();
  if (!rgba_data)
    return false;

  const size_t rgba_data_length = rgba_data->size();
  const size_t rgba_area = width * height;

  if (rgba_data_length != rgba_area * kBytesPerPixel)
    return false;

  SkBitmap bitmap;
  // Allocate the actual backing store with the sanitized dimensions.
  if (!bitmap.tryAllocN32Pixels(width, height))
    return false;

  // Ensure that our bitmap and our data now refer to the same number of pixels.
  if (rgba_data_length != bitmap.computeByteSize())
    return false;

  uint32_t* pixels = bitmap.getAddr32(0, 0);
  const uint8_t* c_rgba_data = rgba_data->data();

  for (size_t t = 0; t < rgba_area; ++t) {
    // |c_rgba_data| is RGBA, pixels is ARGB.
    size_t rgba_index = t * kBytesPerPixel;
    pixels[t] =
        SkPreMultiplyColor(((c_rgba_data[rgba_index + 3] & 0xFF) << 24) |
                           ((c_rgba_data[rgba_index + 0] & 0xFF) << 16) |
                           ((c_rgba_data[rgba_index + 1] & 0xFF) << 8) |
                           ((c_rgba_data[rgba_index + 2] & 0xFF) << 0));
  }

  // TODO(dewittj): Handle HiDPI images with more than one scale factor
  // representation.
  gfx::ImageSkia skia(gfx::ImageSkiaRep(bitmap, 1.0f));
  *return_image = gfx::Image(skia);
  return true;
}

// Returns true if a notification with the given origin should show over the
// currently fullscreen app window. If there is no fullscreen app window,
// returns false.
bool ShouldShowOverCurrentFullscreenWindow(Profile* profile,
                                           const GURL& origin) {
  DCHECK(profile);
  std::string extension_id =
      ExtensionNotificationHandler::GetExtensionId(origin);
  DCHECK(!extension_id.empty());
  AppWindowRegistry::AppWindowList windows =
      AppWindowRegistry::Get(profile)->GetAppWindowsForApp(extension_id);
  for (auto* window : windows) {
    if (window->IsFullscreen() && window->GetBaseWindow()->IsActive())
      return true;
  }
  return false;
}

}  // namespace

bool NotificationsApiFunction::IsNotificationsApiAvailable() {
  // We need to check this explicitly rather than letting
  // _permission_features.json enforce it, because we're sharing the
  // chrome.notifications permissions namespace with WebKit notifications.
  return extension()->is_platform_app() || extension()->is_extension();
}

NotificationsApiFunction::NotificationsApiFunction() {
}

NotificationsApiFunction::~NotificationsApiFunction() {
}

bool NotificationsApiFunction::CreateNotification(
    const std::string& id,
    api::notifications::NotificationOptions* options) {
  // First, make sure the required fields exist: type, title, message, icon.
  // These fields are defined as optional in IDL such that they can be used as
  // optional for notification updates. But for notification creations, they
  // should be present.
  if (options->type == api::notifications::TEMPLATE_TYPE_NONE ||
      !options->icon_url || !options->title || !options->message) {
    SetError(kMissingRequiredPropertiesForCreateNotification);
    return false;
  }

#if !defined(OS_CHROMEOS)
  if (options->priority &&
      *options->priority < message_center::DEFAULT_PRIORITY) {
    SetError(kLowPriorityDeprecatedOnPlatform);
    return false;
  }
#endif

  NotificationBitmapSizes bitmap_sizes = GetNotificationBitmapSizes();

  float image_scale =
      ui::GetScaleForScaleFactor(ui::GetSupportedScaleFactors().back());

  // Extract required fields: type, title, message, and icon.
  message_center::NotificationType type =
      MapApiTemplateTypeToType(options->type);
  UMA_HISTOGRAM_ENUMERATION("Notifications.ExtensionNotificationType", type,
                            message_center::NOTIFICATION_TYPE_LAST);

  const base::string16 title(base::UTF8ToUTF16(*options->title));
  const base::string16 message(base::UTF8ToUTF16(*options->message));
  gfx::Image icon;

  if (!options->icon_bitmap.get() ||
      !NotificationBitmapToGfxImage(
          image_scale, bitmap_sizes.icon_size, *options->icon_bitmap, &icon)) {
    SetError(kUnableToDecodeIconError);
    return false;
  }

  // Then, handle any optional data that's been provided.
  message_center::RichNotificationData optional_fields;
  if (options->app_icon_mask_url.get()) {
    gfx::Image small_icon_mask;
    if (!NotificationBitmapToGfxImage(
            image_scale, bitmap_sizes.app_icon_mask_size,
            *options->app_icon_mask_bitmap, &small_icon_mask)) {
      SetError(kUnableToDecodeIconError);
      return false;
    }
    optional_fields.small_image =
        GetMaskedSmallImage(small_icon_mask.AsImageSkia());
  }

  if (options->priority.get())
    optional_fields.priority = *options->priority;

  if (options->event_time.get())
    optional_fields.timestamp = base::Time::FromJsTime(*options->event_time);

  if (options->silent)
    optional_fields.silent = *options->silent;

  if (options->buttons.get()) {
    // Currently we allow up to 2 buttons.
    size_t number_of_buttons = options->buttons->size();

    // Use distinct buckets for 1-16 notification action buttons, and an
    // overflow bucket for 17 or more action buttons. Does not impact how many
    // action buttons are shown.
    UMA_HISTOGRAM_ENUMERATION("Notifications.ExtensionNotificationActionCount",
                              number_of_buttons, 17);

    number_of_buttons = number_of_buttons > 2 ? 2 : number_of_buttons;

    for (size_t i = 0; i < number_of_buttons; i++) {
      message_center::ButtonInfo info(
          base::UTF8ToUTF16((*options->buttons)[i].title));
      extensions::api::notifications::NotificationBitmap* icon_bitmap_ptr =
          (*options->buttons)[i].icon_bitmap.get();
      if (icon_bitmap_ptr) {
        NotificationBitmapToGfxImage(
            image_scale, bitmap_sizes.button_icon_size, *icon_bitmap_ptr,
            &info.icon);
      }
      optional_fields.buttons.push_back(info);
    }
  }

  if (options->context_message) {
    optional_fields.context_message =
        base::UTF8ToUTF16(*options->context_message);
  }

  bool has_image = options->image_bitmap.get() &&
                   NotificationBitmapToGfxImage(
                       image_scale, bitmap_sizes.image_size,
                       *options->image_bitmap, &optional_fields.image);

  // We should have an image if and only if the type is an image type.
  if (has_image != (type == message_center::NOTIFICATION_TYPE_IMAGE)) {
    SetError(kExtraImageProvided);
    return false;
  }

  // We should have list items if and only if the type is a multiple type.
  bool has_list_items = options->items.get() && !options->items->empty();
  if (has_list_items != (type == message_center::NOTIFICATION_TYPE_MULTIPLE)) {
    SetError(kExtraListItemsProvided);
    return false;
  }

  if (options->progress.get() != NULL) {
    // We should have progress if and only if the type is a progress type.
    if (type != message_center::NOTIFICATION_TYPE_PROGRESS) {
      SetError(kUnexpectedProgressValueForNonProgressType);
      return false;
    }
    optional_fields.progress = *options->progress;
    // Progress value should range from 0 to 100.
    if (optional_fields.progress < 0 || optional_fields.progress > 100) {
      SetError(kInvalidProgressValue);
      return false;
    }
  }

  if (has_list_items) {
    using api::notifications::NotificationItem;
    for (const NotificationItem& api_item : *options->items) {
      optional_fields.items.push_back({base::UTF8ToUTF16(api_item.title),
                                       base::UTF8ToUTF16(api_item.message)});
    }
  }

  optional_fields.settings_button_handler =
      message_center::SettingsButtonHandler::INLINE;

  // TODO(crbug.com/772004): Remove the manual limitation in favor of an IDL
  // annotation once supported.
  if (id.size() > kNotificationIdLengthLimit) {
    SetError(
        base::StringPrintf(kNotificationIdTooLong, kNotificationIdLengthLimit));
    return false;
  }

  std::string notification_id = CreateScopedIdentifier(extension_->id(), id);
  message_center::Notification notification(
      type, notification_id, title, message, icon,
      base::UTF8ToUTF16(extension_->name()), extension_->url(),
      message_center::NotifierId(message_center::NotifierType::APPLICATION,
                                 extension_->id()),
      optional_fields, nullptr /* delegate */);

  // Apply the "requireInteraction" flag. The value defaults to false.
  notification.set_never_timeout(options->require_interaction &&
                                 *options->require_interaction);

  if (ShouldShowOverCurrentFullscreenWindow(GetProfile(),
                                            notification.origin_url())) {
    notification.set_fullscreen_visibility(
        message_center::FullscreenVisibility::OVER_USER);
  }

  GetDisplayHelper()->Display(notification);
  return true;
}

bool NotificationsApiFunction::UpdateNotification(
    const std::string& id,
    api::notifications::NotificationOptions* options,
    message_center::Notification* notification) {
#if !defined(OS_CHROMEOS)
  if (options->priority &&
      *options->priority < message_center::DEFAULT_PRIORITY) {
    SetError(kLowPriorityDeprecatedOnPlatform);
    return false;
  }
#endif

  NotificationBitmapSizes bitmap_sizes = GetNotificationBitmapSizes();
  float image_scale =
      ui::GetScaleForScaleFactor(ui::GetSupportedScaleFactors().back());

  // Update optional fields if provided.
  if (options->type != api::notifications::TEMPLATE_TYPE_NONE)
    notification->set_type(MapApiTemplateTypeToType(options->type));
  if (options->title)
    notification->set_title(base::UTF8ToUTF16(*options->title));
  if (options->message)
    notification->set_message(base::UTF8ToUTF16(*options->message));

  if (options->icon_bitmap.get()) {
    gfx::Image icon;
    if (!NotificationBitmapToGfxImage(
            image_scale, bitmap_sizes.icon_size, *options->icon_bitmap,
            &icon)) {
      SetError(kUnableToDecodeIconError);
      return false;
    }
    notification->set_icon(icon);
  }

  if (options->app_icon_mask_bitmap.get()) {
    gfx::Image app_icon_mask;
    if (!NotificationBitmapToGfxImage(
            image_scale, bitmap_sizes.app_icon_mask_size,
            *options->app_icon_mask_bitmap, &app_icon_mask)) {
      SetError(kUnableToDecodeIconError);
      return false;
    }
    notification->set_small_image(
        GetMaskedSmallImage(app_icon_mask.AsImageSkia()));
  }

  if (options->priority)
    notification->set_priority(*options->priority);

  if (options->event_time)
    notification->set_timestamp(base::Time::FromJsTime(*options->event_time));

  if (options->silent)
    notification->set_silent(*options->silent);

  if (options->buttons) {
    // Currently we allow up to 2 buttons.
    size_t number_of_buttons = options->buttons->size();
    number_of_buttons = number_of_buttons > 2 ? 2 : number_of_buttons;

    std::vector<message_center::ButtonInfo> buttons;
    for (size_t i = 0; i < number_of_buttons; i++) {
      message_center::ButtonInfo button(
          base::UTF8ToUTF16((*options->buttons)[i].title));
      extensions::api::notifications::NotificationBitmap* icon_bitmap_ptr =
          (*options->buttons)[i].icon_bitmap.get();
      if (icon_bitmap_ptr) {
        NotificationBitmapToGfxImage(
            image_scale, bitmap_sizes.button_icon_size, *icon_bitmap_ptr,
            &button.icon);
      }
      buttons.push_back(button);
    }
    notification->set_buttons(buttons);
  }

  if (options->context_message) {
    notification->set_context_message(
        base::UTF8ToUTF16(*options->context_message));
  }

  gfx::Image image;
  bool has_image =
      options->image_bitmap.get() &&
      NotificationBitmapToGfxImage(
          image_scale, bitmap_sizes.image_size, *options->image_bitmap, &image);

  if (has_image) {
    // We should have an image if and only if the type is an image type.
    if (notification->type() != message_center::NOTIFICATION_TYPE_IMAGE) {
      SetError(kExtraImageProvided);
      return false;
    }
    notification->set_image(image);
  }

  if (options->progress) {
    // We should have progress if and only if the type is a progress type.
    if (notification->type() != message_center::NOTIFICATION_TYPE_PROGRESS) {
      SetError(kUnexpectedProgressValueForNonProgressType);
      return false;
    }
    int progress = *options->progress;
    // Progress value should range from 0 to 100.
    if (progress < 0 || progress > 100) {
      SetError(kInvalidProgressValue);
      return false;
    }
    notification->set_progress(progress);
  }

  if (options->items.get() && !options->items->empty()) {
    // We should have list items if and only if the type is a multiple type.
    if (notification->type() != message_center::NOTIFICATION_TYPE_MULTIPLE) {
      SetError(kExtraListItemsProvided);
      return false;
    }

    std::vector<message_center::NotificationItem> items;
    using api::notifications::NotificationItem;
    for (const NotificationItem& api_item : *options->items) {
      items.push_back({base::UTF8ToUTF16(api_item.title),
                       base::UTF8ToUTF16(api_item.message)});
    }
    notification->set_items(items);
  }

  // It's safe to follow the regular path for adding a new notification as it's
  // already been verified that there is a notification that can be updated.
  GetDisplayHelper()->Display(*notification);

  return true;
}

bool NotificationsApiFunction::AreExtensionNotificationsAllowed() const {
  NotifierStateTracker* notifier_state_tracker =
      NotifierStateTrackerFactory::GetForProfile(GetProfile());

  return notifier_state_tracker->IsNotifierEnabled(message_center::NotifierId(
      message_center::NotifierType::APPLICATION, extension_->id()));
}

bool NotificationsApiFunction::IsNotificationsApiEnabled() const {
  return CanRunWhileDisabled() || AreExtensionNotificationsAllowed();
}

bool NotificationsApiFunction::CanRunWhileDisabled() const {
  return false;
}

ExtensionNotificationDisplayHelper* NotificationsApiFunction::GetDisplayHelper()
    const {
  return ExtensionNotificationDisplayHelperFactory::GetForProfile(GetProfile());
}

bool NotificationsApiFunction::RunAsync() {
  if (IsNotificationsApiAvailable() && IsNotificationsApiEnabled()) {
    return RunNotificationsApi();
  } else {
    SendResponse(false);
    return true;
  }
}

message_center::NotificationType
NotificationsApiFunction::MapApiTemplateTypeToType(
    api::notifications::TemplateType type) {
  switch (type) {
    case api::notifications::TEMPLATE_TYPE_NONE:
    case api::notifications::TEMPLATE_TYPE_BASIC:
      return message_center::NOTIFICATION_TYPE_BASE_FORMAT;
    case api::notifications::TEMPLATE_TYPE_IMAGE:
      return message_center::NOTIFICATION_TYPE_IMAGE;
    case api::notifications::TEMPLATE_TYPE_LIST:
      return message_center::NOTIFICATION_TYPE_MULTIPLE;
    case api::notifications::TEMPLATE_TYPE_PROGRESS:
      return message_center::NOTIFICATION_TYPE_PROGRESS;
    default:
      // Gracefully handle newer application code that is running on an older
      // runtime that doesn't recognize the requested template.
      return message_center::NOTIFICATION_TYPE_BASE_FORMAT;
  }
}

NotificationsCreateFunction::NotificationsCreateFunction() {
}

NotificationsCreateFunction::~NotificationsCreateFunction() {
}

bool NotificationsCreateFunction::RunNotificationsApi() {
  params_ = api::notifications::Create::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());

  const std::string extension_id(extension_->id());
  std::string notification_id;
  if (params_->notification_id.get() && !params_->notification_id->empty()) {
    // If the caller provided a notificationId, use that.
    notification_id = *params_->notification_id;
  } else {
    // Otherwise, use a randomly created GUID. In case that GenerateGUID returns
    // the empty string, simply generate a random string.
    notification_id = base::GenerateGUID();
    if (notification_id.empty())
      notification_id = base::RandBytesAsString(16);
  }

  SetResult(std::make_unique<base::Value>(notification_id));

  // TODO(dewittj): Add more human-readable error strings if this fails.
  if (!CreateNotification(notification_id, &params_->options))
    return false;

  SendResponse(true);

  return true;
}

NotificationsUpdateFunction::NotificationsUpdateFunction() {
}

NotificationsUpdateFunction::~NotificationsUpdateFunction() {
}

bool NotificationsUpdateFunction::RunNotificationsApi() {
  params_ = api::notifications::Update::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());

  // We are in update.  If the ID doesn't exist, succeed but call the callback
  // with "false".
  const message_center::Notification* matched_notification =
      GetDisplayHelper()->GetByNotificationId(
          CreateScopedIdentifier(extension_->id(), params_->notification_id));

  if (!matched_notification) {
    SetResult(std::make_unique<base::Value>(false));
    SendResponse(true);
    return true;
  }

  // Copy the existing notification to get a writable version of it.
  message_center::Notification notification = *matched_notification;

  // If we have trouble updating the notification (could be improper use of API
  // or some other reason), mark the function as failed, calling the callback
  // with false.
  // TODO(dewittj): Add more human-readable error strings if this fails.
  bool could_update_notification = UpdateNotification(
      params_->notification_id, &params_->options, &notification);
  SetResult(std::make_unique<base::Value>(could_update_notification));
  if (!could_update_notification)
    return false;

  // No trouble, created the notification, send true to the callback and
  // succeed.
  SendResponse(true);
  return true;
}

NotificationsClearFunction::NotificationsClearFunction() {
}

NotificationsClearFunction::~NotificationsClearFunction() {
}

bool NotificationsClearFunction::RunNotificationsApi() {
  params_ = api::notifications::Clear::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());

  bool cancel_result = GetDisplayHelper()->Close(
      CreateScopedIdentifier(extension_->id(), params_->notification_id));

  SetResult(std::make_unique<base::Value>(cancel_result));
  SendResponse(true);

  return true;
}

NotificationsGetAllFunction::NotificationsGetAllFunction() {}

NotificationsGetAllFunction::~NotificationsGetAllFunction() {}

bool NotificationsGetAllFunction::RunNotificationsApi() {
  std::set<std::string> notification_ids =
      GetDisplayHelper()->GetNotificationIdsForExtension(extension_->url());

  std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue());

  for (auto iter = notification_ids.begin(); iter != notification_ids.end();
       iter++) {
    result->SetKey(StripScopeFromIdentifier(extension_->id(), *iter),
                   base::Value(true));
  }

  SetResult(std::move(result));
  SendResponse(true);

  return true;
}

NotificationsGetPermissionLevelFunction::
NotificationsGetPermissionLevelFunction() {}

NotificationsGetPermissionLevelFunction::
~NotificationsGetPermissionLevelFunction() {}

bool NotificationsGetPermissionLevelFunction::CanRunWhileDisabled() const {
  return true;
}

bool NotificationsGetPermissionLevelFunction::RunNotificationsApi() {
  api::notifications::PermissionLevel result =
      AreExtensionNotificationsAllowed()
          ? api::notifications::PERMISSION_LEVEL_GRANTED
          : api::notifications::PERMISSION_LEVEL_DENIED;

  SetResult(
      std::make_unique<base::Value>(api::notifications::ToString(result)));
  SendResponse(true);

  return true;
}

}  // namespace extensions
