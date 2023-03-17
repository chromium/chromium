// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/telemetry_extension/telemetry_event_service_converters.h"

#include "base/notreached.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_events.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_event_service.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_extension_exception.mojom.h"

namespace ash::converters {

namespace unchecked {

crosapi::mojom::TelemetryAudioJackEventInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::AudioJackEventInfoPtr input) {
  return crosapi::mojom::TelemetryAudioJackEventInfo::New(
      Convert(input->state));
}

crosapi::mojom::TelemetryEventInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::EventInfoPtr input) {
  switch (input->which()) {
    case cros_healthd::mojom::internal::EventInfo_Data::EventInfo_Tag::
        kAudioJackEventInfo:
      return crosapi::mojom::TelemetryEventInfo::NewAudioJackEventInfo(
          ConvertEventPtr(std::move(input->get_audio_jack_event_info())));
    default:
      LOG(WARNING) << "Got event for unsupported category";
      return nullptr;
  }
}

crosapi::mojom::TelemetryExtensionExceptionPtr UncheckedConvertPtr(
    cros_healthd::mojom::ExceptionPtr input) {
  return crosapi::mojom::TelemetryExtensionException::New(
      Convert(input->reason), input->debug_message);
}

crosapi::mojom::TelemetryExtensionSupportedPtr UncheckedConvertPtr(
    cros_healthd::mojom::SupportedPtr input) {
  return crosapi::mojom::TelemetryExtensionSupported::New();
}

crosapi::mojom::TelemetryExtensionUnsupportedReasonPtr UncheckedConvertPtr(
    cros_healthd::mojom::UnsupportedReasonPtr input) {
  switch (input->which()) {
    case cros_healthd::mojom::internal::UnsupportedReason_Data::
        UnsupportedReason_Tag::kUnmappedUnionField:
      return crosapi::mojom::TelemetryExtensionUnsupportedReason::
          NewUnmappedUnionField(input->get_unmapped_union_field());
  }
}

crosapi::mojom::TelemetryExtensionUnsupportedPtr UncheckedConvertPtr(
    cros_healthd::mojom::UnsupportedPtr input) {
  return crosapi::mojom::TelemetryExtensionUnsupported::New(
      input->debug_message, ConvertEventPtr(std::move(input->reason)));
}

crosapi::mojom::TelemetryExtensionSupportStatusPtr UncheckedConvertPtr(
    cros_healthd::mojom::SupportStatusPtr input) {
  switch (input->which()) {
    case cros_healthd::mojom::internal::SupportStatus_Data::SupportStatus_Tag::
        kUnmappedUnionField:
      return crosapi::mojom::TelemetryExtensionSupportStatus::
          NewUnmappedUnionField(input->get_unmapped_union_field());
    case cros_healthd::mojom::internal::SupportStatus_Data::SupportStatus_Tag::
        kException:
      return crosapi::mojom::TelemetryExtensionSupportStatus::NewException(
          ConvertEventPtr(std::move(input->get_exception())));
    case cros_healthd::mojom::internal::SupportStatus_Data::SupportStatus_Tag::
        kSupported:
      return crosapi::mojom::TelemetryExtensionSupportStatus::NewSupported(
          ConvertEventPtr(std::move(input->get_supported())));
    case cros_healthd::mojom::internal::SupportStatus_Data::SupportStatus_Tag::
        kUnsupported:
      return crosapi::mojom::TelemetryExtensionSupportStatus::NewUnsupported(
          ConvertEventPtr(std::move(input->get_unsupported())));
  }
}

}  // namespace unchecked

crosapi::mojom::TelemetryAudioJackEventInfo::State Convert(
    cros_healthd::mojom::AudioJackEventInfo::State input) {
  switch (input) {
    case cros_healthd::mojom::AudioJackEventInfo_State::kUnmappedEnumField:
      return crosapi::mojom::TelemetryAudioJackEventInfo::State::
          kUnmappedEnumField;
    case cros_healthd::mojom::AudioJackEventInfo_State::kAdd:
      return crosapi::mojom::TelemetryAudioJackEventInfo::State::kAdd;
    case cros_healthd::mojom::AudioJackEventInfo_State::kRemove:
      return crosapi::mojom::TelemetryAudioJackEventInfo::State::kRemove;
  }
  NOTREACHED();
}

crosapi::mojom::TelemetryExtensionException::Reason Convert(
    cros_healthd::mojom::Exception::Reason input) {
  switch (input) {
    case cros_healthd::mojom::Exception_Reason::kUnmappedEnumField:
      return crosapi::mojom::TelemetryExtensionException::Reason::
          kUnmappedEnumField;
    case cros_healthd::mojom::Exception_Reason::kMojoDisconnectWithoutReason:
      return crosapi::mojom::TelemetryExtensionException::Reason::
          kMojoDisconnectWithoutReason;
    case cros_healthd::mojom::Exception_Reason::kUnexpected:
      return crosapi::mojom::TelemetryExtensionException::Reason::kUnexpected;
    case cros_healthd::mojom::Exception_Reason::kUnsupported:
      return crosapi::mojom::TelemetryExtensionException::Reason::kUnsupported;
  }
  NOTREACHED();
}

cros_healthd::mojom::EventCategoryEnum Convert(
    crosapi::mojom::TelemetryEventCategoryEnum input) {
  switch (input) {
    case crosapi::mojom::TelemetryEventCategoryEnum::kUnmappedEnumField:
      return cros_healthd::mojom::EventCategoryEnum::kUnmappedEnumField;
    case crosapi::mojom::TelemetryEventCategoryEnum::kAudioJack:
      return cros_healthd::mojom::EventCategoryEnum::kAudioJack;
  }
  NOTREACHED();
}

}  // namespace ash::converters
