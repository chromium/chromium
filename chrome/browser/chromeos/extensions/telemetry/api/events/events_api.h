// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_EVENTS_EVENTS_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_EVENTS_EVENTS_API_H_

#include <optional>

#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/common/base_telemetry_extension_api_guard_function.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/events/remote_event_service_strategy.h"
#include "chromeos/crosapi/mojom/telemetry_event_service.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_extension_exception.mojom.h"
#include "extensions/browser/extension_function.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

class EventsApiFunctionBase : public BaseTelemetryExtensionApiGuardFunction {
 public:
  EventsApiFunctionBase();

 protected:
  ~EventsApiFunctionBase() override;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  bool IsCrosApiAvailable() override;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  mojo::Remote<crosapi::mojom::TelemetryEventService>& GetRemoteService();

  // Gets the parameters passed to the JavaScript call and tries to convert it
  // to the `Params` type. If the `Params` can't be created, this resolves the
  // corresponding JavaScript call with an error and returns `nullopt`.
  template <class Params>
  std::optional<Params> GetParams();
};

class OsEventsIsEventSupportedFunction : public EventsApiFunctionBase {
  DECLARE_EXTENSION_FUNCTION("os.events.isEventSupported",
                             OS_EVENTS_ISEVENTSUPPORTED)

  OsEventsIsEventSupportedFunction() = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;

 private:
  ~OsEventsIsEventSupportedFunction() override = default;
  void OnEventManagerResult(
      crosapi::mojom::TelemetryExtensionSupportStatusPtr status);
};

class OsEventsStartCapturingEventsFunction : public EventsApiFunctionBase {
  DECLARE_EXTENSION_FUNCTION("os.events.startCapturingEvents",
                             OS_EVENTS_STARTCAPTURINGEVENTS)

  OsEventsStartCapturingEventsFunction() = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;

 private:
  ~OsEventsStartCapturingEventsFunction() override = default;
};

class OsEventsStopCapturingEventsFunction : public EventsApiFunctionBase {
  DECLARE_EXTENSION_FUNCTION("os.events.stopCapturingEvents",
                             OS_EVENTS_STOPCAPTURINGEVENTS)

  OsEventsStopCapturingEventsFunction() = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;

 private:
  ~OsEventsStopCapturingEventsFunction() override = default;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_EVENTS_EVENTS_API_H_
