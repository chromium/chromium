// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_ACTUATOR_INTERNALS_BROWSER_ACTUATOR_INTERNALS_MOJOM_TRAITS_H_
#define CHROME_BROWSER_BROWSER_ACTUATOR_INTERNALS_BROWSER_ACTUATOR_INTERNALS_MOJOM_TRAITS_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "chrome/browser/browser_actuator/internals/browser_actuator_internals.mojom-shared.h"
#include "components/stream_transport/internal/session_stream_recorder.h"
#include "components/stream_transport/internal/transport_message_observer.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct EnumTraits<browser_actuator_internals::mojom::DownstreamDeliveryStatus,
                  stream_transport::DownstreamDeliveryStatus> {
  static browser_actuator_internals::mojom::DownstreamDeliveryStatus ToMojom(
      stream_transport::DownstreamDeliveryStatus input);
  static std::optional<stream_transport::DownstreamDeliveryStatus> FromMojom(
      browser_actuator_internals::mojom::DownstreamDeliveryStatus input);
};

template <>
struct StructTraits<browser_actuator_internals::mojom::RecordedEventDataView,
                    stream_transport::SessionEventSnapshot> {
  static base::Time timestamp(const stream_transport::SessionEventSnapshot& e) {
    return e.timestamp;
  }
  static browser_actuator_internals::mojom::MessageDirection direction(
      const stream_transport::SessionEventSnapshot& e);
  static const std::vector<std::string>& payload_types(
      const stream_transport::SessionEventSnapshot& e) {
    return e.payload_types;
  }
  static const std::string& message(
      const stream_transport::SessionEventSnapshot& e) {
    return e.message;
  }
  static bool message_truncated(
      const stream_transport::SessionEventSnapshot& e) {
    return e.message_truncated;
  }
  static int32_t http_response_code(
      const stream_transport::SessionEventSnapshot& e) {
    return e.http_response_code;
  }
  static stream_transport::DownstreamDeliveryStatus downstream_status(
      const stream_transport::SessionEventSnapshot& e) {
    return e.downstream_status;
  }

  static bool Read(
      browser_actuator_internals::mojom::RecordedEventDataView data,
      stream_transport::SessionEventSnapshot* out);
};

template <>
struct StructTraits<browser_actuator_internals::mojom::SessionSummaryDataView,
                    stream_transport::SessionSnapshot> {
  static const std::string& session_id(
      const stream_transport::SessionSnapshot& s) {
    return s.session_id;
  }
  static base::Time start_wall_time(
      const stream_transport::SessionSnapshot& s) {
    return s.start_wall_time;
  }
  static const std::optional<base::Time>& end_wall_time(
      const stream_transport::SessionSnapshot& s) {
    return s.end_wall_time;
  }
  static uint32_t total_downstream_messages(
      const stream_transport::SessionSnapshot& s);
  static uint32_t total_upstream_messages(
      const stream_transport::SessionSnapshot& s);
  static const std::vector<stream_transport::SessionEventSnapshot>& events(
      const stream_transport::SessionSnapshot& s) {
    return s.events;
  }
  static uint32_t total_events(const stream_transport::SessionSnapshot& s);

  static bool Read(
      browser_actuator_internals::mojom::SessionSummaryDataView data,
      stream_transport::SessionSnapshot* out);
};

}  // namespace mojo

#endif  // CHROME_BROWSER_BROWSER_ACTUATOR_INTERNALS_BROWSER_ACTUATOR_INTERNALS_MOJOM_TRAITS_H_
