// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_EVENTS_EVENTS_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_EVENTS_EVENTS_API_H_

#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/common/base_telemetry_extension_api_guard_function.h"
#include "extensions/browser/extension_function.h"

namespace chromeos {

class EventsApiFunctionBase : public BaseTelemetryExtensionApiGuardFunction {
 public:
  EventsApiFunctionBase() = default;

 protected:
  ~EventsApiFunctionBase() override = default;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  bool IsCrosApiAvailable() override;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
};

class OsEventsIsEventSupportedFunction : public EventsApiFunctionBase {
  DECLARE_EXTENSION_FUNCTION("os.events.isEventSupported",
                             OS_EVENTS_ISEVENTSUPPORTED)

  OsEventsIsEventSupportedFunction() = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;

 private:
  ~OsEventsIsEventSupportedFunction() override = default;
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
