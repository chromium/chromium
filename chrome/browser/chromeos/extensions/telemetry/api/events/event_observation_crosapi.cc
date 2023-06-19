// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/events/event_observation_crosapi.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/notreached.h"
#include "base/values.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/events/events_api_converters.h"
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
               crosapi::TelemetryEventInfoPtr info) override {
    std::unique_ptr<extensions::Event> event;
    switch (info->which()) {
      case crosapi::internal::TelemetryEventInfo_Data::TelemetryEventInfo_Tag::
          kDefaultType: {
        LOG(WARNING) << "Got unknown event category";
        return;
      }
      case crosapi::internal::TelemetryEventInfo_Data::TelemetryEventInfo_Tag::
          kAudioJackEventInfo: {
        base::Value::List args;
        args.Append(
            converters::ConvertStructPtr<api::os_events::AudioJackEventInfo>(
                std::move(info->get_audio_jack_event_info()))
                .ToValue());
        event = std::make_unique<extensions::Event>(
            extensions::events::OS_EVENTS_ON_AUDIO_JACK_EVENT,
            api::os_events::OnAudioJackEvent::kEventName, std::move(args),
            browser_context_);
        break;
      }
      case crosapi::internal::TelemetryEventInfo_Data::TelemetryEventInfo_Tag::
          kLidEventInfo: {
        base::Value::List args;
        args.Append(converters::ConvertStructPtr<api::os_events::LidEventInfo>(
                        std::move(info->get_lid_event_info()))
                        .ToValue());
        event = std::make_unique<extensions::Event>(
            extensions::events::OS_EVENTS_ON_LID_EVENT,
            api::os_events::OnLidEvent::kEventName, std::move(args),
            browser_context_);
        break;
      }
      case crosapi::internal::TelemetryEventInfo_Data::TelemetryEventInfo_Tag::
          kUsbEventInfo: {
        base::Value::List args;
        args.Append(converters::ConvertStructPtr<api::os_events::UsbEventInfo>(
                        std::move(info->get_usb_event_info()))
                        .ToValue());
        event = std::make_unique<extensions::Event>(
            extensions::events::OS_EVENTS_ON_USB_EVENT,
            api::os_events::OnUsbEvent::kEventName, std::move(args),
            browser_context_);
        break;
      }
      case crosapi::internal::TelemetryEventInfo_Data::TelemetryEventInfo_Tag::
          kSdCardEventInfo: {
        base::Value::List args;
        args.Append(
            converters::ConvertStructPtr<api::os_events::SdCardEventInfo>(
                std::move(info->get_sd_card_event_info()))
                .ToValue());
        event = std::make_unique<extensions::Event>(
            extensions::events::OS_EVENTS_ON_SD_CARD_EVENT,
            api::os_events::OnSdCardEvent::kEventName, std::move(args),
            browser_context_);
        break;
      }
      case crosapi::internal::TelemetryEventInfo_Data::TelemetryEventInfo_Tag::
          kPowerEventInfo: {
        base::Value::List args;
        args.Append(
            converters::ConvertStructPtr<api::os_events::PowerEventInfo>(
                std::move(info->get_power_event_info()))
                .ToValue());
        event = std::make_unique<extensions::Event>(
            extensions::events::OS_EVENTS_ON_POWER_EVENT,
            api::os_events::OnPowerEvent::kEventName, std::move(args),
            browser_context_);
        break;
      }
      case crosapi::internal::TelemetryEventInfo_Data::TelemetryEventInfo_Tag::
          kKeyboardDiagnosticEventInfo: {
        base::Value::List args;
        args.Append(converters::ConvertStructPtr<
                        api::os_events::KeyboardDiagnosticEventInfo>(
                        std::move(info->get_keyboard_diagnostic_event_info()))
                        .ToValue());

        event = std::make_unique<extensions::Event>(
            extensions::events::OS_EVENTS_ON_KEYBOARD_DIAGNOSTIC_EVENT,
            api::os_events::OnKeyboardDiagnosticEvent::kEventName,
            std::move(args), browser_context_);
        break;
      }
      case crosapi::internal::TelemetryEventInfo_Data::TelemetryEventInfo_Tag::
          kStylusGarageEventInfo: {
        base::Value::List args;
        args.Append(
            converters::ConvertStructPtr<api::os_events::StylusGarageEventInfo>(
                std::move(info->get_stylus_garage_event_info()))
                .ToValue());
        event = std::make_unique<extensions::Event>(
            extensions::events::OS_EVENTS_ON_STYLUS_GARAGE_EVENT,
            api::os_events::OnStylusGarageEvent::kEventName, std::move(args),
            browser_context_);
        break;
      }
      case crosapi::internal::TelemetryEventInfo_Data::TelemetryEventInfo_Tag::
          kTouchpadButtonEventInfo: {
        base::Value::List args;
        args.Append(converters::ConvertStructPtr<
                        api::os_events::TouchpadButtonEventInfo>(
                        std::move(info->get_touchpad_button_event_info()))
                        .ToValue());
        event = std::make_unique<extensions::Event>(
            extensions::events::OS_EVENTS_ON_TOUCHPAD_BUTTON_EVENT,
            api::os_events::OnTouchpadButtonEvent::kEventName, std::move(args),
            browser_context_);
        break;
      }
      case crosapi::internal::TelemetryEventInfo_Data::TelemetryEventInfo_Tag::
          kTouchpadTouchEventInfo: {
        base::Value::List args;
        args.Append(converters::ConvertStructPtr<
                        api::os_events::TouchpadTouchEventInfo>(
                        std::move(info->get_touchpad_touch_event_info()))
                        .ToValue());
        event = std::make_unique<extensions::Event>(
            extensions::events::OS_EVENTS_ON_TOUCHPAD_TOUCH_EVENT,
            api::os_events::OnTouchpadTouchEvent::kEventName, std::move(args),
            browser_context_);
        break;
      }
      case crosapi::internal::TelemetryEventInfo_Data::TelemetryEventInfo_Tag::
          kTouchpadConnectedEventInfo: {
        base::Value::List args;
        args.Append(converters::ConvertStructPtr<
                        api::os_events::TouchpadConnectedEventInfo>(
                        std::move(info->get_touchpad_connected_event_info()))
                        .ToValue());
        event = std::make_unique<extensions::Event>(
            extensions::events::OS_EVENTS_ON_TOUCHPAD_CONNECTED_EVENT,
            api::os_events::OnTouchpadConnectedEvent::kEventName,
            std::move(args), browser_context_);
        break;
      }
    }

    extensions::EventRouter::Get(browser_context_)
        ->DispatchEventToExtension(extension_id, std::move(event));
  }

 private:
  raw_ptr<content::BrowserContext, DanglingUntriaged> browser_context_;
};

}  // namespace

EventObservationCrosapi::EventObservationCrosapi(
    const extensions::ExtensionId& extension_id,
    content::BrowserContext* context)
    : extension_id_(extension_id),
      receiver_(this),
      delegate_(std::make_unique<DefaultEventDelegate>(context)),
      browser_context_(context) {}

EventObservationCrosapi::~EventObservationCrosapi() = default;

void EventObservationCrosapi::OnEvent(crosapi::TelemetryEventInfoPtr info) {
  if (!info) {
    LOG(WARNING) << "Received empty event";
    return;
  }

  delegate_->OnEvent(extension_id_, std::move(info));
}

mojo::PendingRemote<crosapi::TelemetryEventObserver>
EventObservationCrosapi::GetRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

}  // namespace chromeos
