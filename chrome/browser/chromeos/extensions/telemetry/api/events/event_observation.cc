// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/events/event_observation.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/notreached.h"
#include "base/values.h"
#include "chrome/common/chromeos/extensions/api/events.h"
#include "chromeos/ash/components/telemetry_extension/events/telemetry_event_service_converters.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/common/extension_id.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace chromeos {

namespace {

class DefaultEventDelegate : public EventObservation::Delegate {
 public:
  DefaultEventDelegate(content::BrowserContext* context,
                       api::os_events::EventCategory category)
      : browser_context_(context), category_(category) {}
  ~DefaultEventDelegate() override = default;

  void OnEvent(const extensions::ExtensionId& extension_id,
               EventRouter* event_router,
               ash::cros_healthd::mojom::EventInfoPtr healthd_info) override {
    if (!event_router->IsExtensionAllowedForCategory(extension_id, category_)) {
      return;
    }

    // TODO(crbug.com/508411965): Remove the use of crosapi struct.
    auto info =
        ash::converters::events::ConvertStructPtr(std::move(healthd_info));
    std::unique_ptr<extensions::Event> event;
    switch (category_) {
      case api::os_events::EventCategory::kNone: {
        return;
      }
      case api::os_events::EventCategory::kAudioJack: {
        CHECK(info->is_audio_jack_event_info());
        event = std::make_unique<extensions::Event>(
            extensions::events::OS_EVENTS_ON_AUDIO_JACK_EVENT,
            api::os_events::OnAudioJackEvent::kEventName,
            base::ListValue().Append(
                converters::events::ConvertStructPtr(
                    std::move(info->get_audio_jack_event_info()))
                    .ToValue()),
            browser_context_);
        break;
      }
      case api::os_events::EventCategory::kLid: {
        CHECK(info->is_lid_event_info());
        event = std::make_unique<extensions::Event>(
            extensions::events::OS_EVENTS_ON_LID_EVENT,
            api::os_events::OnLidEvent::kEventName,
            base::ListValue().Append(converters::events::ConvertStructPtr(
                                         std::move(info->get_lid_event_info()))
                                         .ToValue()),
            browser_context_);
        break;
      }
      case api::os_events::EventCategory::kUsb: {
        CHECK(info->is_usb_event_info());
        event = std::make_unique<extensions::Event>(
            extensions::events::OS_EVENTS_ON_USB_EVENT,
            api::os_events::OnUsbEvent::kEventName,
            base::ListValue().Append(converters::events::ConvertStructPtr(
                                         std::move(info->get_usb_event_info()))
                                         .ToValue()),
            browser_context_);
        break;
      }
      case api::os_events::EventCategory::kExternalDisplay: {
        CHECK(info->is_external_display_event_info());
        event = std::make_unique<extensions::Event>(
            extensions::events::OS_EVENTS_ON_EXTERNAL_DISPLAY_EVENT,
            api::os_events::OnExternalDisplayEvent::kEventName,
            base::ListValue().Append(
                converters::events::ConvertStructPtr(
                    std::move(info->get_external_display_event_info()))
                    .ToValue()),
            browser_context_);
        break;
      }
      case api::os_events::EventCategory::kSdCard: {
        CHECK(info->is_sd_card_event_info());
        event = std::make_unique<extensions::Event>(
            extensions::events::OS_EVENTS_ON_SD_CARD_EVENT,
            api::os_events::OnSdCardEvent::kEventName,
            base::ListValue().Append(
                converters::events::ConvertStructPtr(
                    std::move(info->get_sd_card_event_info()))
                    .ToValue()),
            browser_context_);
        break;
      }
      case api::os_events::EventCategory::kPower: {
        CHECK(info->is_power_event_info());
        event = std::make_unique<extensions::Event>(
            extensions::events::OS_EVENTS_ON_POWER_EVENT,
            api::os_events::OnPowerEvent::kEventName,
            base::ListValue().Append(
                converters::events::ConvertStructPtr(
                    std::move(info->get_power_event_info()))
                    .ToValue()),
            browser_context_);
        break;
      }
      case api::os_events::EventCategory::kKeyboardDiagnostic: {
        CHECK(info->is_keyboard_diagnostic_event_info());
        event = std::make_unique<extensions::Event>(
            extensions::events::OS_EVENTS_ON_KEYBOARD_DIAGNOSTIC_EVENT,
            api::os_events::OnKeyboardDiagnosticEvent::kEventName,
            base::ListValue().Append(
                converters::events::ConvertStructPtr(
                    std::move(info->get_keyboard_diagnostic_event_info()))
                    .ToValue()),
            browser_context_);
        break;
      }
      case api::os_events::EventCategory::kStylusGarage: {
        CHECK(info->is_stylus_garage_event_info());
        event = std::make_unique<extensions::Event>(
            extensions::events::OS_EVENTS_ON_STYLUS_GARAGE_EVENT,
            api::os_events::OnStylusGarageEvent::kEventName,
            base::ListValue().Append(
                converters::events::ConvertStructPtr(
                    std::move(info->get_stylus_garage_event_info()))
                    .ToValue()),
            browser_context_);
        break;
      }
      case api::os_events::EventCategory::kTouchpadButton: {
        if (!info->is_touchpad_button_event_info()) {
          return;
        }
        event = std::make_unique<extensions::Event>(
            extensions::events::OS_EVENTS_ON_TOUCHPAD_BUTTON_EVENT,
            api::os_events::OnTouchpadButtonEvent::kEventName,
            base::ListValue().Append(
                converters::events::ConvertStructPtr(
                    std::move(info->get_touchpad_button_event_info()))
                    .ToValue()),
            browser_context_);
        break;
      }
      case api::os_events::EventCategory::kTouchpadTouch: {
        if (!info->is_touchpad_touch_event_info()) {
          return;
        }
        event = std::make_unique<extensions::Event>(
            extensions::events::OS_EVENTS_ON_TOUCHPAD_TOUCH_EVENT,
            api::os_events::OnTouchpadTouchEvent::kEventName,
            base::ListValue().Append(
                converters::events::ConvertStructPtr(
                    std::move(info->get_touchpad_touch_event_info()))
                    .ToValue()),
            browser_context_);
        break;
      }
      case api::os_events::EventCategory::kTouchpadConnected: {
        if (!info->is_touchpad_connected_event_info()) {
          return;
        }
        event = std::make_unique<extensions::Event>(
            extensions::events::OS_EVENTS_ON_TOUCHPAD_CONNECTED_EVENT,
            api::os_events::OnTouchpadConnectedEvent::kEventName,
            base::ListValue().Append(
                converters::events::ConvertStructPtr(
                    std::move(info->get_touchpad_connected_event_info()))
                    .ToValue()),
            browser_context_);
        break;
      }
      case api::os_events::EventCategory::kTouchscreenTouch: {
        if (!info->is_touchscreen_touch_event_info()) {
          return;
        }
        event = std::make_unique<extensions::Event>(
            extensions::events::OS_EVENTS_ON_TOUCHSCREEN_TOUCH_EVENT,
            api::os_events::OnTouchscreenTouchEvent::kEventName,
            base::ListValue().Append(
                converters::events::ConvertStructPtr(
                    std::move(info->get_touchscreen_touch_event_info()))
                    .ToValue()),
            browser_context_);
        break;
      }
      case api::os_events::EventCategory::kTouchscreenConnected: {
        if (!info->is_touchscreen_connected_event_info()) {
          return;
        }
        event = std::make_unique<extensions::Event>(
            extensions::events::OS_EVENTS_ON_TOUCHSCREEN_CONNECTED_EVENT,
            api::os_events::OnTouchscreenConnectedEvent::kEventName,
            base::ListValue().Append(
                converters::events::ConvertStructPtr(
                    std::move(info->get_touchscreen_connected_event_info()))
                    .ToValue()),
            browser_context_);
        break;
      }
      case api::os_events::EventCategory::kStylusTouch: {
        if (!info->is_stylus_touch_event_info()) {
          return;
        }
        event = std::make_unique<extensions::Event>(
            extensions::events::OS_EVENTS_ON_STYLUS_TOUCH_EVENT,
            api::os_events::OnStylusTouchEvent::kEventName,
            base::ListValue().Append(
                converters::events::ConvertStructPtr(
                    std::move(info->get_stylus_touch_event_info()))
                    .ToValue()),
            browser_context_);
        break;
      }
      case api::os_events::EventCategory::kStylusConnected: {
        if (!info->is_stylus_connected_event_info()) {
          return;
        }
        event = std::make_unique<extensions::Event>(
            extensions::events::OS_EVENTS_ON_STYLUS_CONNECTED_EVENT,
            api::os_events::OnStylusConnectedEvent::kEventName,
            base::ListValue().Append(
                converters::events::ConvertStructPtr(
                    std::move(info->get_stylus_connected_event_info()))
                    .ToValue()),
            browser_context_);
        break;
      }
    }

    extensions::EventRouter::Get(browser_context_)
        ->DispatchEventToExtension(extension_id, std::move(event));
  }

 private:
  const raw_ptr<content::BrowserContext, DanglingUntriaged> browser_context_;
  const api::os_events::EventCategory category_;
};

}  // namespace

EventObservation::EventObservation(const extensions::ExtensionId& extension_id,
                                   api::os_events::EventCategory category,
                                   EventRouter* event_router,
                                   content::BrowserContext* context)
    : extension_id_(extension_id),
      receiver_(this),
      delegate_(std::make_unique<DefaultEventDelegate>(context, category)),
      event_router_(event_router),
      browser_context_(context) {}

EventObservation::~EventObservation() = default;

void EventObservation::OnEvent(ash::cros_healthd::mojom::EventInfoPtr info) {
  if (!info) {
    LOG(WARNING) << "Received empty event";
    return;
  }

  delegate_->OnEvent(extension_id_, event_router_, std::move(info));
}

mojo::PendingRemote<ash::cros_healthd::mojom::EventObserver>
EventObservation::GetRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

}  // namespace chromeos
