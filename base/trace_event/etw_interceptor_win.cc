// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/etw_interceptor_win.h"

#include <optional>
#include <type_traits>

#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event_etw_export_win.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/perfetto/protos/perfetto/common/interceptor_descriptor.gen.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/track_event.pbzero.h"

namespace base::trace_event {

namespace {
template <typename T>
concept EtwFieldWithDataDescType = EtwFieldBaseType<T> && requires(T t) {
  { t.GetDataDescCount() } -> std::same_as<uint8_t>;
};

template <typename T, typename = void>
struct EventDataDescTraits;

template <typename T>
struct EventDataDescTraits<T, std::enable_if_t<std::is_arithmetic_v<T>>> {
  static const T* GetAddress(const T& value) noexcept {
    return const_cast<T*>(&value);
  }
  static ULONG GetSize(const T& value) noexcept { return sizeof(value); }
};

template <>
struct EventDataDescTraits<std::string> {
  static const char* GetAddress(const std::string& value) noexcept {
    return value.c_str();
  }
  static ULONG GetSize(const std::string& value) noexcept {
    return static_cast<ULONG>(value.size() + 1);
  }
};

class TlmFieldDebugAnnotation final : public TlmFieldBase {
 public:
  TlmFieldDebugAnnotation(
      std::string_view name,
      perfetto::protos::pbzero::DebugAnnotation_Decoder& annotation);
  ~TlmFieldDebugAnnotation();

  void FillEventDescriptor(EVENT_DATA_DESCRIPTOR* descriptors) const noexcept;

  uint8_t GetDataDescCount() const noexcept;
  uint8_t GetInType() const noexcept;
  uint8_t GetOutType() const noexcept;

  // Copy operations are suppressed. Only declare move operations.
  TlmFieldDebugAnnotation(TlmFieldDebugAnnotation&&) noexcept;
  TlmFieldDebugAnnotation& operator=(TlmFieldDebugAnnotation&&) noexcept;

 private:
  uint8_t data_desc_count_ = 1;
  uint8_t in_type_ = 2 /* TlgInANSISTRING */;
  uint8_t out_type_ = 0;
  absl::variant<std::string, uint64_t, int64_t, bool, double> value_;
};

TlmFieldDebugAnnotation::TlmFieldDebugAnnotation(
    std::string_view name,
    perfetto::protos::pbzero::DebugAnnotation_Decoder& annotation)
    : TlmFieldBase(name) {
  CHECK_NE(Name().data(), nullptr);

  if (annotation.has_bool_value()) {
    in_type_ = 4 /* TlgInUINT8 */;
    out_type_ = 3 /* TlgOutBOOLEAN */;
    value_ = annotation.bool_value();
  } else if (annotation.has_int_value()) {
    in_type_ = 9;
    value_ = annotation.int_value();
  } else if (annotation.has_uint_value()) {
    in_type_ = 10;
    value_ = annotation.uint_value();
  } else if (annotation.has_string_value()) {
    in_type_ = 2 /* TlgInANSISTRING */;
    value_.emplace<std::string>(annotation.string_value().data,
                                annotation.string_value().size);
  } else if (annotation.has_legacy_json_value()) {
    in_type_ = 2 /* TlgInANSISTRING */;
    value_.emplace<std::string>(annotation.legacy_json_value().data,
                                annotation.legacy_json_value().size);
  } else if (annotation.has_pointer_value()) {
    in_type_ = 21 /* TlgInINTPTR */;
    value_ = annotation.pointer_value();
  } else if (annotation.has_double_value()) {
    in_type_ = 12 /* TlgInDOUBLE */;
    value_ = annotation.double_value();
  }
}

TlmFieldDebugAnnotation::~TlmFieldDebugAnnotation() = default;

TlmFieldDebugAnnotation::TlmFieldDebugAnnotation(
    TlmFieldDebugAnnotation&&) noexcept = default;
TlmFieldDebugAnnotation& TlmFieldDebugAnnotation::operator=(
    TlmFieldDebugAnnotation&&) noexcept = default;

void TlmFieldDebugAnnotation::FillEventDescriptor(
    EVENT_DATA_DESCRIPTOR* descriptors) const noexcept {
  absl::visit(
      [&]<typename T>(const T& arg) {
        using Traits = EventDataDescTraits<T>;
        EventDataDescCreate(&descriptors[0], Traits::GetAddress(arg),
                            Traits::GetSize(arg));
      },
      value_);
}

uint8_t TlmFieldDebugAnnotation::GetDataDescCount() const noexcept {
  return data_desc_count_;
}

uint8_t TlmFieldDebugAnnotation::GetInType() const noexcept {
  return in_type_;
}

uint8_t TlmFieldDebugAnnotation::GetOutType() const noexcept {
  return out_type_;
}

std::string_view GetDebugAnnotationName(
    perfetto::TrackEventStateTracker::SequenceState& sequence_state,
    const perfetto::protos::pbzero::DebugAnnotation_Decoder& annotation) {
  protozero::ConstChars name{};
  if (const auto id = annotation.name_iid()) {
    const auto& value = sequence_state.debug_annotation_names[id];
    name.data = value.data();
    name.size = value.size();
  } else if (annotation.has_name()) {
    name.data = annotation.name().data;
    name.size = annotation.name().size;
  }
  return std::string_view(name.data, name.size);
}
}  // namespace

class MultiEtwPayloadHandler final {
 public:
  MultiEtwPayloadHandler(TlmProvider* provider,
                         std::string_view event_name,
                         const EVENT_DESCRIPTOR& event_descriptor)
      : provider_(provider), event_descriptor_(event_descriptor) {
    is_enabled_ = provider_->IsEnabled(event_descriptor);
    if (!is_enabled_) {
      return;
    }
    metadata_index_ = provider_->EventBegin(metadata_, event_name);
  }

  // Ensures that this function cannot be called with temporary objects.
  template <EtwFieldWithDataDescType T>
  void WriteField(const T&& value) = delete;

  // Caller needs to ensure that the `value` being passed is not destroyed, till
  // `EventEnd` is called.
  template <EtwFieldWithDataDescType T>
  void WriteField(const T& value) {
    if (!is_enabled_) {
      return;
    }
    const int data_desc_count = value.GetDataDescCount();
    provider_->EventAddField(metadata_, &metadata_index_, value.GetInType(),
                             value.GetOutType(), value.Name());
    descriptors_.resize(descriptors_.size() +
                        static_cast<size_t>(data_desc_count));

    value.FillEventDescriptor(&descriptors_[descriptors_index_]);
    descriptors_index_ += data_desc_count;
  }

  ULONG EventEnd() {
    if (!is_enabled_) {
      return 0;
    }
    ULONG ret =
        provider_->EventEnd(metadata_, metadata_index_, &descriptors_[0],
                            descriptors_index_, event_descriptor_);
    return ret;
  }

 private:
  raw_ptr<TlmProvider> provider_;
  bool is_enabled_ = false;
  char metadata_[TlmProvider::kMaxEventMetadataSize]{};
  uint16_t metadata_index_ = 0;
  static constexpr int kMaxPossibleDescriptors = 6;
  static constexpr int kMinPossibleDescriptors = 2;
  uint8_t descriptors_index_ = kMinPossibleDescriptors;
  absl::InlinedVector<EVENT_DATA_DESCRIPTOR, kMaxPossibleDescriptors>
      descriptors_{kMinPossibleDescriptors};
  EVENT_DESCRIPTOR event_descriptor_;
};

class ETWInterceptor::Delegate
    : public perfetto::TrackEventStateTracker::Delegate {
 public:
  Delegate(perfetto::LockedHandle<ETWInterceptor> locked_self,
           perfetto::TrackEventStateTracker::SequenceState& sequence_state)
      : sequence_state_(sequence_state), locked_self_(std::move(locked_self)) {
    DCHECK(locked_self_);
  }
  ~Delegate() override;

  perfetto::TrackEventStateTracker::SessionState* GetSessionState() override;
  void OnTrackUpdated(perfetto::TrackEventStateTracker::Track&) override;
  void OnTrackEvent(
      const perfetto::TrackEventStateTracker::Track&,
      const perfetto::TrackEventStateTracker::ParsedTrackEvent&) override;

 private:
  raw_ref<perfetto::TrackEventStateTracker::SequenceState> sequence_state_;
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
  // TODO(crbug.com/40276149): Consider exporting thread time once
  // TrackEventStateTracker supports it.
  if (!event.track_event.has_debug_annotations()) {
    if (event.track_event.type() ==
        perfetto::protos::pbzero::TrackEvent::TYPE_SLICE_END) {
      locked_self_->provider_->WriteEvent(
          std::string_view(event.name.data, event.name.size),
          TlmEventDescriptor(0, keyword),
          TlmMbcsStringField("Phase", phase_string),
          TlmUInt64Field("Id", track.uuid),
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
          TlmUInt64Field("Id", track.uuid),
          TlmUInt64Field("Timestamp",
                         event.timestamp_ns /
                             base::TimeTicks::kNanosecondsPerMicrosecond));
    }
  } else {
    const auto event_descriptor = TlmEventDescriptor(0, keyword);
    const std::string_view event_name(event.name.data, event.name.size);

    MultiEtwPayloadHandler etw_payload_handler(locked_self_->provider_,
                                               event_name, event_descriptor);
    const TlmMbcsStringField phase_event("Phase", phase_string);
    etw_payload_handler.WriteField(phase_event);

    const TlmUInt64Field timestamp_field(
        "Timestamp",
        event.timestamp_ns / base::TimeTicks::kNanosecondsPerMicrosecond);
    etw_payload_handler.WriteField(timestamp_field);

    const TlmUInt64Field id_field("Id", track.uuid);
    etw_payload_handler.WriteField(id_field);

    std::optional<TlmUInt64Field> duration_field;
    if (event.track_event.type() ==
        perfetto::protos::pbzero::TrackEvent::TYPE_SLICE_END) {
      duration_field.emplace(
          "Duration",
          event.duration_ns / base::TimeTicks::kNanosecondsPerMicrosecond);
      etw_payload_handler.WriteField(*duration_field);
    }

    // Add debug annotations.
    static constexpr int kMaxDebugAnnotations = 2;
    absl::InlinedVector<TlmFieldDebugAnnotation, kMaxDebugAnnotations>
        debug_fields;
    for (auto it = event.track_event.debug_annotations(); it; ++it) {
      perfetto::protos::pbzero::DebugAnnotation_Decoder annotation(*it);
      debug_fields.emplace_back(
          GetDebugAnnotationName(sequence_state_.get(), annotation),
          annotation);
    }
    for (const auto& debug_field : debug_fields) {
      etw_payload_handler.WriteField(debug_field);
    }
    etw_payload_handler.EventEnd();
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
  perfetto::LockedHandle<ETWInterceptor> locked_self =
      context.GetInterceptorLocked();
  if (!locked_self) {
    return;
  }
  Delegate delegate(std::move(locked_self), tls.sequence_state);
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
