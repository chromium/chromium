// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/etw_interceptor_win.h"

#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event_etw_export_win.h"
#include "third_party/perfetto/protos/perfetto/common/interceptor_descriptor.gen.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/track_event.pbzero.h"

namespace base::trace_event {

class ETWInterceptor::Delegate
    : public perfetto::TrackEventStateTracker::Delegate {
 public:
  explicit Delegate(InterceptorContext& context)
      : context_(&context), locked_self_(context_->GetInterceptorLocked()) {}
  ~Delegate() override;

  perfetto::TrackEventStateTracker::SessionState* GetSessionState() override;
  void OnTrackUpdated(perfetto::TrackEventStateTracker::Track&) override;
  void OnTrackEvent(
      const perfetto::TrackEventStateTracker::Track&,
      const perfetto::TrackEventStateTracker::ParsedTrackEvent&) override;

 private:
  raw_ptr<InterceptorContext> context_;
  perfetto::LockedHandle<ETWInterceptor> locked_self_;
};

ETWInterceptor::Delegate::~Delegate() = default;

perfetto::TrackEventStateTracker::SessionState*
ETWInterceptor::Delegate::GetSessionState() {
  return &locked_self_->session_state_;
}

void ETWInterceptor::Delegate::OnTrackUpdated(
    perfetto::TrackEventStateTracker::Track& track) {}

void ETWInterceptor::Delegate::OnTrackEvent(
    const perfetto::TrackEventStateTracker::Track& track,
    const perfetto::TrackEventStateTracker::ParsedTrackEvent& event) {
  uint64_t keyword = base::trace_event::CategoryGroupToETWKeyword(
      std::string_view(event.category.data, event.category.size));
  const char* phase_string = nullptr;
  switch (event.track_event.type()) {
    case perfetto::protos::pbzero::TrackEvent::TYPE_SLICE_BEGIN:
      phase_string = "Begin";
      break;
    case perfetto::protos::pbzero::TrackEvent::TYPE_SLICE_END:
      phase_string = "End";
      break;
    case perfetto::protos::pbzero::TrackEvent::TYPE_INSTANT:
      phase_string = "Instant";
      break;
  }
  DCHECK_NE(nullptr, phase_string);
  // TODO(crbug.com/1465855): Consider exporting thread time once
  // TrackEventStateTracker supports it.
  if (event.track_event.type() ==
      perfetto::protos::pbzero::TrackEvent::TYPE_SLICE_END) {
    locked_self_->provider_->WriteEvent(
        std::string_view(event.name.data, event.name.size),
        TlmEventDescriptor(0, keyword),
        TlmMbcsStringField("Phase", phase_string),
        TlmUInt64Field(
            "Timestamp",
            event.timestamp_ns / base::TimeTicks::kNanosecondsPerMicrosecond),
        TlmUInt64Field(
            "Duration",
            event.duration_ns / base::TimeTicks::kNanosecondsPerMicrosecond));
  } else {
    locked_self_->provider_->WriteEvent(
        std::string_view(event.name.data, event.name.size),
        TlmEventDescriptor(0, keyword),
        TlmMbcsStringField("Phase", phase_string),
        TlmUInt64Field(
            "Timestamp",
            event.timestamp_ns / base::TimeTicks::kNanosecondsPerMicrosecond));
  }
}

ETWInterceptor::ETWInterceptor(TlmProvider* provider) : provider_(provider) {}
ETWInterceptor::~ETWInterceptor() = default;

void ETWInterceptor::Register(TlmProvider* provider) {
  perfetto::protos::gen::InterceptorDescriptor desc;
  desc.set_name("etwexport");
  perfetto::Interceptor<ETWInterceptor>::Register(desc, provider);
}

void ETWInterceptor::OnTracePacket(InterceptorContext context) {
  auto& tls = context.GetThreadLocalState();
  Delegate delegate(context);
  perfetto::protos::pbzero::TracePacket::Decoder packet(
      context.packet_data.data, context.packet_data.size);
  perfetto::TrackEventStateTracker::ProcessTracePacket(
      delegate, tls.sequence_state, packet);
}

ETWInterceptor::ThreadLocalState::ThreadLocalState(ThreadLocalStateArgs& args) {
}
ETWInterceptor::ThreadLocalState::~ThreadLocalState() = default;

void ETWInterceptor::OnSetup(const SetupArgs&) {}
void ETWInterceptor::OnStart(const StartArgs&) {}
void ETWInterceptor::OnStop(const StopArgs&) {}

}  // namespace base::trace_event
