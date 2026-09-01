// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_metrics.h"

#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "components/sharing_message/proto/glic_experimental_triggering.pb.h"

namespace glic {

namespace {

// Must match the Channel token variants declared for
// Glic.ExperimentalTriggering.IncomingMessageResult.{Channel} in
// //tools/metrics/histograms/metadata/glic/histograms.xml.
std::string_view ChannelVariant(
    ScopedIncomingMessageResultLogger::Channel channel) {
  switch (channel) {
    case ScopedIncomingMessageResultLogger::Channel::kSharingMessage:
      return "SharingMessage";
    case ScopedIncomingMessageResultLogger::Channel::kBrowserActuatorTransport:
      return "BrowserActuatorTransport";
  }
  NOTREACHED();
}

std::string IncomingMessageResultHistogramName(
    ScopedIncomingMessageResultLogger::Channel channel) {
  return base::StrCat({"Glic.ExperimentalTriggering.IncomingMessageResult.",
                       ChannelVariant(channel)});
}

}  // namespace

ScopedIncomingMessageResultLogger::ScopedIncomingMessageResultLogger(
    Channel channel)
    : channel_(channel) {}

ScopedIncomingMessageResultLogger::ScopedIncomingMessageResultLogger(
    ScopedIncomingMessageResultLogger&& other) noexcept
    : channel_(other.channel_),
      result_(std::move(other.result_)),
      disarmed_(std::exchange(other.disarmed_, true)) {}

ScopedIncomingMessageResultLogger& ScopedIncomingMessageResultLogger::operator=(
    ScopedIncomingMessageResultLogger&& other) noexcept {
  if (this != &other) {
    LogAndDisarm();
    channel_ = other.channel_;
    result_ = std::move(other.result_);
    disarmed_ = std::exchange(other.disarmed_, true);
  }
  return *this;
}

ScopedIncomingMessageResultLogger::~ScopedIncomingMessageResultLogger() {
  LogAndDisarm();
}

void ScopedIncomingMessageResultLogger::LogAndDisarm() {
  // If this object was moved from, disarmed_ is true: logging responsibility
  // was transferred to another instance, so there is nothing to do.
  if (disarmed_) {
    return;
  }
  // Every processing exit path is expected to set a result. Fall back to
  // kResultNotSet rather than dropping the sample, so the histogram records
  // exactly one sample per incoming message and instrumentation gaps are
  // visible in released builds instead of silently undercounting.
  DCHECK(result_.has_value());
  base::UmaHistogramEnumeration(
      IncomingMessageResultHistogramName(channel_),
      result_.value_or(
          GlicExperimentalTriggeringIncomingMessageResult::kResultNotSet));
  disarmed_ = true;
}

void ScopedIncomingMessageResultLogger::set_result(
    GlicExperimentalTriggeringIncomingMessageResult result) {
  result_ = result;
}

void MaybeRecordInitialSharingMessageDeliveryLatency(
    const components_sharing_message::GlicExperimentalTriggering& triggering) {
  if (!triggering.has_request()) {
    return;
  }
  const auto& request = triggering.request();
  // Only log for the first sharing message arriving in Chrome (OptIn or
  // TriggerActuation).
  if (!request.has_device_opt_in_request() &&
      !request.has_trigger_actuation_request()) {
    return;
  }
  if (!triggering.has_task_metadata() ||
      !triggering.task_metadata().has_server_time_stamp()) {
    return;
  }
  const auto& timestamp = triggering.task_metadata().server_time_stamp();
  if (timestamp.seconds() <= 0 || timestamp.nanos() < 0 ||
      timestamp.nanos() >= base::Time::kNanosecondsPerSecond) {
    return;
  }
  base::Time server_time = base::Time::UnixEpoch() +
                           base::Seconds(timestamp.seconds()) +
                           base::Nanoseconds(timestamp.nanos());
  base::TimeDelta latency = base::Time::Now() - server_time;
  if (latency.is_negative()) {
    latency = base::TimeDelta();
  }
  base::UmaHistogramCustomTimes(
      "Glic.ExperimentalTriggering.FirstFCMMessageLatency", latency,
      base::Milliseconds(100), base::Days(10), 100);
}

}  // namespace glic
