// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_EXPERIMENTAL_TRIGGERING_GLIC_EXPERIMENTAL_TRIGGERING_CONVERTERS_H_
#define CHROME_BROWSER_GLIC_EXPERIMENTAL_TRIGGERING_GLIC_EXPERIMENTAL_TRIGGERING_CONVERTERS_H_

#include <map>
#include <optional>
#include <string>
#include <string_view>

#include "base/time/time.h"
#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_types.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "components/sharing_message/proto/glic_experimental_triggering.pb.h"
#include "components/sharing_message/proto/sharing_message.pb.h"

namespace glic {

class GlicEnabling;

// `TaskUpdate.metadata` keys populated by Chrome for terminal failure
// attribution and milestone latency diagnostics.
inline constexpr std::string_view kMetadataChromeErrorCodeKey =
    "chrome_error_code";
// Carries an opaque, human-readable diagnostic string formatted inside Glic
// (`GlicInvokeHandler`) on invocation failure. Forwarded opaquely to
// downstream server-side (non-UMA) diagnostic logging only; callers must not
// parse or branch on its contents.
inline constexpr std::string_view kMetadataChromeInvokeDiagnosticsKey =
    "chrome_invoke_diagnostics";
inline constexpr std::string_view kMetadataPanelOpenedKey = "panel_opened";
inline constexpr std::string_view kMetadataClientConnectedKey =
    "client_connected";
inline constexpr std::string_view kMetadataTimeToPanelOpenMsKey =
    "t_to_panel_open_ms";
inline constexpr std::string_view kMetadataTimeToClientConnectedMsKey =
    "t_to_client_connected_ms";
inline constexpr std::string_view kMetadataTimeToFirstResponseMsKey =
    "t_to_first_response_ms";
inline constexpr std::string_view kMetadataTimeToTerminalMsKey =
    "t_to_terminal_ms";

// Bounded, enumerated `TaskUpdate.metadata["chrome_error_code"]` values emitted
// when Chrome terminates an actuation request without a WebUI-supplied error.
// NOTE: These wire tokens are consumed by remote caller retry logic and
// downstream server-side (non-UMA) telemetry/alerting. Do not rename or alter
// the meaning of existing values without coordinating with downstream owners.
inline constexpr std::string_view kErrorCodeGlicInvokeTimeout =
    "GLIC_INVOKE_TIMEOUT";
inline constexpr std::string_view kErrorCodeGlicClientLoadError =
    "GLIC_CLIENT_LOAD_ERROR";
inline constexpr std::string_view kErrorCodeGlicTabClosed = "GLIC_TAB_CLOSED";
inline constexpr std::string_view kErrorCodeGlicPanelClosed =
    "GLIC_PANEL_CLOSED";
inline constexpr std::string_view kErrorCodeGlicSuperseded = "GLIC_SUPERSEDED";
inline constexpr std::string_view kErrorCodeGlicClientDisconnected =
    "GLIC_CLIENT_DISCONNECTED";
inline constexpr std::string_view kErrorCodeGlicDestroyedBeforeCompletion =
    "GLIC_DESTROYED_BEFORE_COMPLETION";
inline constexpr std::string_view kErrorCodeEtNoBrowserWindow =
    "ET_NO_BROWSER_WINDOW";
inline constexpr std::string_view kErrorCodeEtCannotActOnWeb =
    "ET_CANNOT_ACT_ON_WEB";

// Extracts domain `TaskMetadata` from a `GlicExperimentalTriggering` protobuf
// if present.
std::optional<TaskMetadata> ProtoToTaskMetadata(
    const components_sharing_message::GlicExperimentalTriggering& proto);

// Converts a raw `GlicExperimentalTriggering` protobuf to a domain
// `ExperimentalTriggeringRequest`.
ExperimentalTriggeringRequest ProtoToRequest(
    const components_sharing_message::GlicExperimentalTriggering& proto);

// Converts a domain `ExperimentalTriggeringResponse` into a
// `GlicExperimentalTriggering` protobuf.
components_sharing_message::GlicExperimentalTriggering
ResponseToTriggeringProto(const ExperimentalTriggeringResponse& response);

// Converts a domain `ExperimentalTriggeringResponse` back into a
// `SharingMessage` protobuf for transmission.
components_sharing_message::SharingMessage ResponseToProto(
    const ExperimentalTriggeringResponse& response);

// Maps selected `GlicInvokeError` values to their wire `chrome_error_code`
// string, or `std::nullopt` for unlisted errors that omit `chrome_error_code`.
std::optional<std::string_view> GlicInvokeErrorToChromeErrorCode(
    GlicInvokeError error);

// Builds a `TaskUpdate.metadata` map containing `chrome_error_code`.
std::map<std::string, std::string> BuildChromeErrorMetadata(
    std::string_view error_code);

// Builds `TaskUpdate.metadata` for requests rejected because experimental
// triggering is unavailable or not ready, emitting `ET_CANNOT_ACT_ON_WEB` only
// when `enabling.GetExperimentalTriggeringNotReadyReason()` is
// `kCannotActOnWeb`.
std::map<std::string, std::string> BuildUnavailableTriggeringMetadata(
    const GlicEnabling& enabling);

// Builds Chrome failure attribution `TaskUpdate.metadata` entries from a
// `GlicInvokeError` and opaque `diagnostic_info` string.
std::map<std::string, std::string> BuildInvokeErrorMetadata(
    GlicInvokeError error,
    std::string_view diagnostic_info);

// Formats a non-negative millisecond duration string for `TaskUpdate.metadata`.
// Negative deltas (e.g. from synthetic/test `TimeDelta` inputs) are clamped to
// `"0"` so the wire metadata representation is guaranteed non-negative.
std::string FormatMetadataDurationMs(base::TimeDelta duration);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_EXPERIMENTAL_TRIGGERING_GLIC_EXPERIMENTAL_TRIGGERING_CONVERTERS_H_
