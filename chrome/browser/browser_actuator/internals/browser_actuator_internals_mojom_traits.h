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
#include "components/browser_actuator/internal/session_stream_recorder.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct StructTraits<browser_actuator_internals::mojom::RecordedEventDataView,
                    browser_actuator::SessionEventSnapshot> {
  static base::Time timestamp(const browser_actuator::SessionEventSnapshot& e) {
    return e.timestamp;
  }
  static browser_actuator_internals::mojom::MessageDirection direction(
      const browser_actuator::SessionEventSnapshot& e);
  static const std::vector<std::string>& payload_types(
      const browser_actuator::SessionEventSnapshot& e) {
    return e.payload_types;
  }
  static const std::string& message(
      const browser_actuator::SessionEventSnapshot& e) {
    return e.message;
  }
  static bool message_truncated(
      const browser_actuator::SessionEventSnapshot& e) {
    return e.message_truncated;
  }

  static bool Read(
      browser_actuator_internals::mojom::RecordedEventDataView data,
      browser_actuator::SessionEventSnapshot* out);
};

template <>
struct StructTraits<browser_actuator_internals::mojom::SessionSummaryDataView,
                    browser_actuator::SessionSnapshot> {
  static const std::string& session_id(
      const browser_actuator::SessionSnapshot& s) {
    return s.session_id;
  }
  static base::Time start_wall_time(
      const browser_actuator::SessionSnapshot& s) {
    return s.start_wall_time;
  }
  static const std::optional<base::Time>& end_wall_time(
      const browser_actuator::SessionSnapshot& s) {
    return s.end_wall_time;
  }
  static uint32_t total_downstream_messages(
      const browser_actuator::SessionSnapshot& s);
  static uint32_t total_upstream_messages(
      const browser_actuator::SessionSnapshot& s);
  static const std::vector<browser_actuator::SessionEventSnapshot>& events(
      const browser_actuator::SessionSnapshot& s) {
    return s.events;
  }
  static uint32_t total_events(const browser_actuator::SessionSnapshot& s);

  static bool Read(
      browser_actuator_internals::mojom::SessionSummaryDataView data,
      browser_actuator::SessionSnapshot* out);
};

}  // namespace mojo

#endif  // CHROME_BROWSER_BROWSER_ACTUATOR_INTERNALS_BROWSER_ACTUATOR_INTERNALS_MOJOM_TRAITS_H_
