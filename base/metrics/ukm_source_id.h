// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_METRICS_UKM_SOURCE_ID_H_
#define BASE_METRICS_UKM_SOURCE_ID_H_

#include <stdint.h>

#include "base/base_export.h"

namespace base {

// An ID used to identify a Source to UKM, for recording information about it.
// These objects are copyable, assignable, and occupy 64-bits per instance.
// Prefer passing them by value. When a new type is added, please also update
// the enum type in third_party/metrics_proto/ukm/source.proto and the
// converting function ToProtobufSourceType.
class BASE_EXPORT UkmSourceId {
 public:
  enum class Type : int64_t {
    // Source ids of this type are created via ukm::AssignNewSourceId, to denote
    // 'custom' source other than the types below. Source of this type has
    // additional restrictions with logging, as determined by
    // IsWhitelistedSourceId.
    DEFAULT = 0,
    // Sources created by navigation. They will be kept in memory as long as
    // the associated tab is still alive and the number of sources are within
    // the max threshold.
    NAVIGATION_ID = 1,
    // Source ID used by AppLaunchEventLogger::Log. A new source of this type
    // and associated events are expected to be recorded within the same report
    // interval; it will not be kept in memory between different reports.
    APP_ID = 2,
    // Source ID for background events that don't have an open tab but the
    // associated URL is still present in the browsing history. A new source of
    // this type and associated events are expected to be recorded within the
    // same report interval; it will not be kept in memory between different
    // reports.
    HISTORY_ID = 3,
    // Source ID used by WebApkUkmRecorder. A new source of this type and
    // associated events are expected to be recorded within the same report
    // interval; it will not be kept in memory between different reports.
    WEBAPK_ID = 4,
    // Source ID for service worker based payment handlers. A new source of this
    // type and associated events are expected to be recorded within the same
    // report interval; it will not be kept in memory between different reports.
    PAYMENT_APP_ID = 5,
    // Source ID for desktop web apps, based on the start_url in the web app
    // manifest. A new source of this type and associated events are expected to
    // be recorded within the same report interval; it will not be kept in
    // memory between different reports.
    DESKTOP_WEB_APP_ID = 6,
    // Source ID for web workers, namely SharedWorkers and ServiceWorkers. Web
    // workers may inherit a source ID from the spawner context (in the case of
    // dedicated workers), or may have their own source IDs (in the case of
    // shared workers and service workers). Shared workers and service workers
    // can be connected to multiple clients (e.g. documents or other workers).
    WORKER_ID = 7,
    kMaxValue = WORKER_ID,
  };

  // Default constructor has the invalid value.
  constexpr UkmSourceId() : value_(0) {}

  constexpr UkmSourceId& operator=(UkmSourceId other) {
    value_ = other.value_;
    return *this;
  }

  // Allow identity comparisons.
  constexpr bool operator==(UkmSourceId other) const {
    return value_ == other.value_;
  }
  constexpr bool operator!=(UkmSourceId other) const {
    return value_ != other.value_;
  }

  // Allow coercive comparisons to simplify test migration.
  // TODO(crbug/873866): Remove these once callers are migrated.
  constexpr bool operator==(int64_t other) const { return value_ == other; }
  constexpr bool operator!=(int64_t other) const { return value_ == other; }

  // Extract the Type of the SourceId.
  Type GetType() const;

  // Return the ID as an int64.
  constexpr int64_t ToInt64() const { return value_; }

  // Convert an int64 ID value to an ID.
  static constexpr UkmSourceId FromInt64(int64_t internal_value) {
    return UkmSourceId(internal_value);
  }

  // Get a new UKM-Type SourceId, which is unique within the scope of a
  // browser session.
  static UkmSourceId New();

  // Utility for converting other unique ids to source ids.
  static UkmSourceId FromOtherId(int64_t value, Type type);

 private:
  constexpr explicit UkmSourceId(int64_t value) : value_(value) {}
  int64_t value_;
};

constexpr UkmSourceId kInvalidUkmSourceId = UkmSourceId();

}  // namespace base

#endif  // BASE_METRICS_UKM_SOURCE_ID_H_
