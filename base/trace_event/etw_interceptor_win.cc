// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/etw_interceptor_win.h"

#include <array>
#include <optional>
#include <type_traits>
#include <variant>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "base/strings/string_tokenizer.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"
#include "third_party/perfetto/protos/perfetto/common/interceptor_descriptor.gen.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/debug_annotation.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/track_event.pbzero.h"

namespace base::trace_event {

namespace {

// |kFilteredEventGroupNames| contains the event categories that can be
// exported individually. These categories can be enabled by passing the correct
// keyword when starting the trace. A keyword is a 64-bit flag and we attribute
// one bit per category. We can therefore enable a particular category by
// setting its corresponding bit in the keyword. For events that are not present
// in |kFilteredEventGroupNames|, we have two bits that control their
// behaviour. When bit 46 is enabled, any event that is not disabled by default
// (ie. doesn't start with disabled-by-default-) will be exported. Likewise,
// when bit 47 is enabled, any event that is disabled by default will be
// exported.
//
// Examples of passing keywords to the provider using xperf:
// # This exports "benchmark" and "cc" events
// xperf -start chrome -on Chrome:0x9
//
// # This exports "gpu", "netlog" and all other events that are not disabled by
// # default
// xperf -start chrome -on Chrome:0x4000000000A0
//
// More info about starting a trace and keyword can be obtained by using the
// help section of xperf (xperf -help start). Note that xperf documentation
// refers to keywords as flags and there are two ways to enable them, using
// group names or the hex representation. We only support the latter. Also, we
// ignore the level.
//
// To avoid continually having to bump MSEdge values to next higher bits, we
// are putting MSEdge values at the high end of the bit range and will grow
// 'down' to lower bits for future MSEdge entries.
//
// As the writing of this comment, we have 4 values:
//    "navigation",                                       // 0x40000000000
//    "ServiceWorker",                                    // 0x80000000000
//    "edge_webview",                                     // 0x100000000000
//    "diagnostic_event",                                 // 0x200000000000
//
// This means the next value added should be:
//    "the_next_value",                                   // 0x20000000000
//    "navigation",                                       // 0x40000000000
//    "ServiceWorker",                                    // 0x80000000000
//    "edge_webview",                                     // 0x100000000000
//    "diagnostic_event",                                 // 0x200000000000
//
// The addition of the "unused_bit_nn" entries keeps the existing code execution
// routines working (ex. TraceEventETWExport::UpdateEnabledCategories()) and
// enables others to see which bits are available.
//
// Example: For some new category group...
//   "latency",                                          // 0x8000
//   "blink.user_timing",                                // 0x10000
//   "unused_bit_18",                                    // 0x20000
//   "unused_bit_19",                                    // 0x40000
//   "unused_bit_20",                                    // 0x80000
//    ...
// becomes:
//   "latency",                                          // 0x8000
//   "blink.user_timing",                                // 0x10000
//   "new_upstream_value",                               // 0x20000
//   "unused_bit_19",                                    // 0x40000
//   "unused_bit_20",                                    // 0x80000
//
// The high 16 bits of the keyword have special semantics and should not be
// set for enabling individual categories as they are reserved by winmeta.xml.
constexpr std::array<const char*, 64> kFilteredEventGroupNames = {
    "benchmark",                             // 0x1
    "blink",                                 // 0x2
    "browser",                               // 0x4
    "cc",                                    // 0x8
    "evdev",                                 // 0x10
    "gpu",                                   // 0x20
    "input",                                 // 0x40
    "netlog",                                // 0x80
    "sequence_manager",                      // 0x100
    "toplevel",                              // 0x200
    "v8",                                    // 0x400
    "disabled-by-default-cc.debug",          // 0x800
    "disabled-by-default-cc.debug.picture",  // 0x1000
    "disabled-by-default-toplevel.flow",     // 0x2000
    "startup",                               // 0x4000
    "latency",                               // 0x8000
    "blink.user_timing",                     // 0x10000
    "media",                                 // 0x20000
    "loading",                               // 0x40000
    "base",                                  // 0x80000
    "devtools.timeline",                     // 0x100000
    "mediastream",                           // 0x200000
    "blink_style",                           // 0x400000
    "unused_bit_23",                         // 0x800000
    "unused_bit_24",                         // 0x1000000
    "unused_bit_25",                         // 0x2000000
    "unused_bit_26",                         // 0x4000000
    "unused_bit_27",                         // 0x8000000
    "unused_bit_28",                         // 0x10000000
    "unused_bit_29",                         // 0x20000000
    "unused_bit_30",                         // 0x40000000
    "unused_bit_31",                         // 0x80000000
    "unused_bit_32",                         // 0x100000000
    "unused_bit_33",                         // 0x200000000
    "unused_bit_34",                         // 0x400000000
    "unused_bit_35",                         // 0x800000000
    "unused_bit_36",                         // 0x1000000000
    "unused_bit_37",                         // 0x2000000000
    "unused_bit_38",                         // 0x4000000000
    "unused_bit_39",                         // 0x8000000000
    "unused_bit_40",                         // 0x10000000000
    "unused_bit_41",                         // 0x20000000000
    "navigation",                            // 0x40000000000
    "ServiceWorker",                         // 0x80000000000
    "edge_webview",                          // 0x100000000000
    "diagnostic_event",                      // 0x200000000000
    "__OTHER_EVENTS",                        // 0x400000000000 See below
    "__DISABLED_OTHER_EVENTS",               // 0x800000000000 See below
};

// These must be kept as the last two entries in the above array.
constexpr uint8_t kOtherEventsGroupNameIndex = 46;
constexpr uint8_t kDisabledOtherEventsGroupNameIndex = 47;

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
  std::variant<std::string, uint64_t, int64_t, bool, double> value_;
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
  std::visit(
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

uint64_t CategoryGroupToETWKeyword(std::string_view category_group_name) {
  static NoDestructor<base::flat_map<std::string_view, uint64_t>>
      categories_to_keyword([] {
        std::vector<std::pair<std::string_view, uint64_t>> items;
        for (size_t i = 0; i < kOtherEventsGroupNameIndex; i++) {
          uint64_t keyword = 1ULL << i;
          items.emplace_back(kFilteredEventGroupNames[i], keyword);
        }
        std::sort(items.begin(), items.end());
        return base::flat_map<std::string_view, uint64_t>(base::sorted_unique,
                                                          std::move(items));
      }());

  uint64_t keyword = 0;

  // To enable multiple sessions with this provider enabled we need to log the
  // level and keyword with the event so that if the sessions differ in the
  // level or keywords enabled we log the right events and allow ETW to
  // route the data to the appropriate session.
  // TODO(joel@microsoft.com) Explore better methods in future integration
  // with perfetto.

  StringViewTokenizer category_group_tokens(category_group_name.begin(),
                                            category_group_name.end(), ",");
  while (category_group_tokens.GetNext()) {
    std::string_view category_group_token = category_group_tokens.token_piece();

    // Lookup the keyword for this part of the category_group_name
    // and or in the keyword.
    auto it = categories_to_keyword->find(category_group_token);
    if (it != categories_to_keyword->end()) {
      keyword |= it->second;
    } else {
      if (StartsWith(category_group_token, "disabled-by-default")) {
        keyword |= (1ULL << kDisabledOtherEventsGroupNameIndex);
      } else {
        keyword |= (1ULL << kOtherEventsGroupNameIndex);
      }
    }
  }
  return keyword;
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

perfetto::protos::gen::TrackEventConfig ETWKeywordToTrackEventConfig(
    uint64_t keyword) {
  perfetto::protos::gen::TrackEventConfig track_event_config;
  for (size_t i = 0; i < kOtherEventsGroupNameIndex; ++i) {
    if (keyword & (1ULL << i)) {
      track_event_config.add_enabled_categories(kFilteredEventGroupNames[i]);
    }
  }
  bool other_events_enabled = (keyword & (1ULL << kOtherEventsGroupNameIndex));
  bool disabled_other_events_enables =
      (keyword & (1ULL << kDisabledOtherEventsGroupNameIndex));
  if (other_events_enabled) {
    track_event_config.add_enabled_categories("*");
  } else {
    track_event_config.add_disabled_categories("*");
  }
  if (!disabled_other_events_enables) {
    track_event_config.add_disabled_categories("disabled-by-default-*");
  } else if (other_events_enabled) {
    track_event_config.add_enabled_categories("disabled-by-default-*");
  }
  return track_event_config;
}

}  // namespace base::trace_event
