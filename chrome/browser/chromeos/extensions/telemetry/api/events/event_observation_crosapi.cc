// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/events/event_observation_crosapi.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/notreached.h"
#include "base/values.h"
#include "chrome/common/chromeos/extensions/api/events.h"
#include "chromeos/crosapi/mojom/telemetry_event_service.mojom.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/common/extension_id.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace chromeos {

namespace {

namespace crosapi = ::crosapi::mojom;

class DefaultEventDelegate : public EventObservationCrosapi::Delegate {
 public:
  explicit DefaultEventDelegate(content::BrowserContext* context)
      : browser_context_(context) {}
  ~DefaultEventDelegate() override = default;

  void OnEvent(const extensions::ExtensionId& extension_id,
               EventRouter* event_router,
               crosapi::TelemetryEventInfoPtr info) override {
    std::unique_ptr<extensions::Event> event;
    crosapi::TelemetryEventCategoryEnum category;
    switch (info->which()) {
      case crosapi::TelemetryEventInfo::Tag::kDefaultType: {
        LOG(WARNING) << "Got unknown event category";
        return;
      }
      case crosapi::TelemetryEventInfo::Tag::kAudioJackEventInfo: {
        category = crosapi::TelemetryEventCategoryEnum::kAudioJack;
        event = std::make_unique<extensions::Event>(
            extensions::events::OS_EVENTS_ON_AUDIO_JACK_EVENT,
            api::os_events::OnAudioJackEvent::kEventName,
            base::Value::List().Append(
                converters::events::ConvertStructPtr(
                    std::move(info->get_audio_jack_event_info()))
                    .ToValue()),
            browser_context_);
        break;
      }
      case crosapi::TelemetryEventInfo::Tag::kLidEventInfo: {
        category = crosapi::TelemetryEventCategoryEnum::kLid;
        event = std::make_unique<extensions::Event>(
            extensions::events::OS_EVENTS_ON_LID_EVENT,
            api::os_events::OnLidEvent::kEventName,
            base::Value::List().Append(
                converters::events::ConvertStructPtr(
                    std::move(info->get_lid_event_info()))
                    .ToValue()),
            browser_context_);
        break;
      }
      case crosapi::TelemetryEventInfo::Tag::kUsbEventInfo: {
        category = crosapi::TelemetryEventCategoryEnum::kUsb;
        event = std::make_unique<extensions::Event>(
            extensions::events::OS_EVENTS_ON_USB_EVENT,
            api::os_events::OnUsbEvent::kEventName,
            base::Value::List().Append(
                converters::events::ConvertStructPtr(
                    std::move(info->get_usb_event_info()))
                    .ToValue()),
            browser_context_);
        break;
      }
      case crosapi::TelemetryEventInfo::Tag::kExternalDisplayEventInfo: {
        category = crosapi::TelemetryEventCategoryEnum::kExternalDisplay;
        event = std::make_unique<extensions::Event>(
            extensions::events::OS_EVENTS_ON_EXTERNAL_DISPLAY_EVENT,
            api::os_events::OnExternalDisplayEvent::kEventName,
            base::Value::List().Append(
                converters::events::ConvertStructPtr(
                    std::move(info->get_external_display_event_info()))
                    .ToValue()),
            browser_context_);
        break;
      }
      case crosapi::TelemetryEventInfo::Tag::kSdCardEventInfo: {
        category = crosapi::TelemetryEventCategoryEnum::kSdCard;
        event = std::make_unique<extensions::Event>(
            extensions::events::OS_EVENTS_ON_SD_CARD_EVENT,
            api::os_events::OnSdCardEvent::kEventName,
            base::Value::List().Append(
                converters::events::ConvertStructPtr(
                    std::move(info->get_sd_card_event_info()))
                    .ToValue()),
            browser_context_);
        break;
      }
      case crosapi::TelemetryEventInfo::Tag::kPowerEventInfo: {
        category = crosapi::TelemetryEventCategoryEnum::kPower;
        event = std::make_unique<extensions::Event>(
            extensions::events::OS_EVENTS_ON_POWER_EVENT,
            api::os_events::OnPowerEvent::kEventName,
            base::Value::List().Append(
                converters::events::ConvertStructPtr(
                    std::move(info->get_power_event_info()))
                    .ToValue()),
            browser_context_);
        break;
      }
      case crosapi::TelemetryEventInfo::Tag::kKeyboardDiagnosticEventInfo: {
        category = crosapi::TelemetryEventCategoryEnum::kKeyboardDiagnostic;
        event = std::make_unique<extensions::Event>(
            extensions::events::OS_EVENTS_ON_KEYBOARD_DIAGNOSTIC_EVENT,
            api::os_events::OnKeyboardDiagnosticEvent::kEventName,
            base::Value::List().Append(
                converters::events::ConvertStructPtr(
                    std::move(info->get_keyboard_diagnostic_event_info()))
                    .ToValue()),
            browser_context_);
        break;
      }
      case crosapi::TelemetryEventInfo::Tag::kStylusGarageEventInfo: {
        category = crosapi::TelemetryEventCategoryEnum::kStylusGarage;
        event = std::make_unique<extensions::Event>(
            extensions::events::OS_EVENTS_ON_STYLUS_GARAGE_EVENT,
            api::os_events::OnStylusGarageEvent::kEventName,
            base::Value::List().Append(
                converters::events::ConvertStructPtr(
                    std::move(info->get_stylus_garage_event_info()))
                    .ToValue()),
            browser_context_);
        break;
      }
      case crosapi::TelemetryEventInfo::Tag::kTouchpadButtonEventInfo: {
        category = crosapi::TelemetryEventCategoryEnum::kTouchpadButton;
        event = std::make_unique<extensions::Event>(
            extensions::events::OS_EVENTS_ON_TOUCHPAD_BUTTON_EVENT,
            api::os_events::OnTouchpadButtonEvent::kEventName,
            base::Value::List().Append(
                converters::events::ConvertStructPtr(
                    std::move(info->get_touchpad_button_event_info()))
                    .ToValue()),
            browser_context_);
        break;
      }
      case crosapi::TelemetryEventInfo::Tag::kTouchpadTouchEventInfo: {
        category = crosapi::TelemetryEventCategoryEnum::kTouchpadTouch;
        event = std::make_unique<extensions::Event>(
            extensions::events::OS_EVENTS_ON_TOUCHPAD_TOUCH_EVENT,
            api::os_events::OnTouchpadTouchEvent::kEventName,
            base::Value::List().Append(
                converters::events::ConvertStructPtr(
                    std::move(info->get_touchpad_touch_event_info()))
                    .ToValue()),
            browser_context_);
        break;
      }
      case crosapi::TelemetryEventInfo::Tag::kTouchpadConnectedEventInfo: {
        category = crosapi::TelemetryEventCategoryEnum::kTouchpadConnected;
        event = std::make_unique<extensions::Event>(
            extensions::events::OS_EVENTS_ON_TOUCHPAD_CONNECTED_EVENT,
            api::os_events::OnTouchpadConnectedEvent::kEventName,
            base::Value::List().Append(
                converters::events::ConvertStructPtr(
                    std::move(info->get_touchpad_connected_event_info()))
                    .ToValue()),
            browser_context_);
        break;
      }
      case crosapi::TelemetryEventInfo::Tag::kTouchscreenTouchEventInfo: {
        category = crosapi::TelemetryEventCategoryEnum::kTouchscreenTouch;
        event = std::make_unique<extensions::Event>(
            extensions::events::OS_EVENTS_ON_TOUCHSCREEN_TOUCH_EVENT,
            api::os_events::OnTouchscreenTouchEvent::kEventName,
            base::Value::List().Append(
                converters::events::ConvertStructPtr(
                    std::move(info->get_touchscreen_touch_event_info()))
                    .ToValue()),
            browser_context_);
        break;
      }
      case crosapi::TelemetryEventInfo::Tag::kTouchscreenConnectedEventInfo: {
        category = crosapi::TelemetryEventCategoryEnum::kTouchscreenConnected;
        event = std::make_unique<extensions::Event>(
            extensions::events::OS_EVENTS_ON_TOUCHSCREEN_CONNECTED_EVENT,
            api::os_events::OnTouchscreenConnectedEvent::kEventName,
            base::Value::List().Append(
                converters::events::ConvertStructPtr(
                    std::move(info->get_touchscreen_connected_event_info()))
                    .ToValue()),
            browser_context_);
        break;
      }
      case crosapi::TelemetryEventInfo::Tag::kStylusTouchEventInfo: {
        category = crosapi::TelemetryEventCategoryEnum::kStylusTouch;
        event = std::make_unique<extensions::Event>(
            extensions::events::OS_EVENTS_ON_STYLUS_TOUCH_EVENT,
            api::os_events::OnStylusTouchEvent::kEventName,
            base::Value::List().Append(
                converters::events::ConvertStructPtr(
                    std::move(info->get_stylus_touch_event_info()))
                    .ToValue()),
            browser_context_);
        break;
      }
      case crosapi::TelemetryEventInfo::Tag::kStylusConnectedEventInfo: {
        category = crosapi::TelemetryEventCategoryEnum::kStylusConnected;
        event = std::make_unique<extensions::Event>(
            extensions::events::OS_EVENTS_ON_STYLUS_CONNECTED_EVENT,
            api::os_events::OnStylusConnectedEvent::kEventName,
            base::Value::List().Append(
                converters::events::ConvertStructPtr(
                    std::move(info->get_stylus_connected_event_info()))
                    .ToValue()),
            browser_context_);
        break;
      }
    }

    if (event_router->IsExtensionAllowedForCategory(extension_id, category)) {
      extensions::EventRouter::Get(browser_context_)
          ->DispatchEventToExtension(extension_id, std::move(event));
    }
  }

 private:
  raw_ptr<content::BrowserContext, DanglingUntriaged> browser_context_;
};

}  // namespace

EventObservationCrosapi::EventObservationCrosapi(
    const extensions::ExtensionId& extension_id,
    EventRouter* event_router,
    content::BrowserContext* context)
    : extension_id_(extension_id),
      receiver_(this),
      delegate_(std::make_unique<DefaultEventDelegate>(context)),
      event_router_(event_router),
      browser_context_(context) {}

EventObservationCrosapi::~EventObservationCrosapi() = default;

void EventObservationCrosapi::OnEvent(crosapi::TelemetryEventInfoPtr info) {
  if (!info) {
    LOG(WARNING) << "Received empty event";
    return;
  }

  delegate_->OnEvent(extension_id_, event_router_, std::move(info));
}

mojo::PendingRemote<crosapi::TelemetryEventObserver>
EventObservationCrosapi::GetRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

}  // namespace chromeos
