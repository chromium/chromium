// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/trace_event_impl.h"

#include <stddef.h>

#include "base/format_macros.h"
#include "base/json/string_escape.h"
#include "base/memory/ptr_util.h"
#include "base/process/process_handle.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_log.h"
#include "base/trace_event/traced_value.h"

namespace base {
namespace trace_event {

namespace {

size_t GetAllocLength(const char* str) { return str ? strlen(str) + 1 : 0; }

// Copies |*member| into |*buffer|, sets |*member| to point to this new
// location, and then advances |*buffer| by the amount written.
void CopyTraceEventParameter(char** buffer,
                             const char** member,
                             const char* end) {
  if (*member) {
    size_t written = strlcpy(*buffer, *member, end - *buffer) + 1;
    DCHECK_LE(static_cast<int>(written), end - *buffer);
    *member = *buffer;
    *buffer += written;
  }
}

}  // namespace

TraceEvent::TraceEvent()
    : duration_(TimeDelta::FromInternalValue(-1)),
      scope_(trace_event_internal::kGlobalScope),
      id_(0u),
      category_group_enabled_(nullptr),
      name_(nullptr),
      thread_id_(0),
      flags_(0),
      phase_(TRACE_EVENT_PHASE_BEGIN) {
  for (int i = 0; i < kTraceMaxNumArgs; ++i)
    arg_names_[i] = nullptr;
  memset(arg_values_, 0, sizeof(arg_values_));
}

TraceEvent::~TraceEvent() = default;

void TraceEvent::MoveFrom(std::unique_ptr<TraceEvent> other) {
  timestamp_ = other->timestamp_;
  thread_timestamp_ = other->thread_timestamp_;
  duration_ = other->duration_;
  scope_ = other->scope_;
  id_ = other->id_;
  category_group_enabled_ = other->category_group_enabled_;
  name_ = other->name_;
  if (other->flags_ & TRACE_EVENT_FLAG_HAS_PROCESS_ID)
    process_id_ = other->process_id_;
  else
    thread_id_ = other->thread_id_;
  phase_ = other->phase_;
  flags_ = other->flags_;
  parameter_copy_storage_ = std::move(other->parameter_copy_storage_);

  for (int i = 0; i < kTraceMaxNumArgs; ++i) {
    arg_names_[i] = other->arg_names_[i];
    arg_types_[i] = other->arg_types_[i];
    arg_values_[i] = other->arg_values_[i];
    convertable_values_[i] = std::move(other->convertable_values_[i]);
  }
}

void TraceEvent::Initialize(
    int thread_id,
    TimeTicks timestamp,
    ThreadTicks thread_timestamp,
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    const char* scope,
    unsigned long long id,
    unsigned long long bind_id,
    int num_args,
    const char* const* arg_names,
    const unsigned char* arg_types,
    const unsigned long long* arg_values,
    std::unique_ptr<ConvertableToTraceFormat>* convertable_values,
    unsigned int flags) {
  timestamp_ = timestamp;
  thread_timestamp_ = thread_timestamp;
  duration_ = TimeDelta::FromInternalValue(-1);
  scope_ = scope;
  id_ = id;
  category_group_enabled_ = category_group_enabled;
  name_ = name;
  thread_id_ = thread_id;
  phase_ = phase;
  flags_ = flags;
  bind_id_ = bind_id;

  // Clamp num_args since it may have been set by a third_party library.
  num_args = (num_args > kTraceMaxNumArgs) ? kTraceMaxNumArgs : num_args;
  int i = 0;
  for (; i < num_args; ++i) {
    arg_names_[i] = arg_names[i];
    arg_types_[i] = arg_types[i];

    if (arg_types[i] == TRACE_VALUE_TYPE_CONVERTABLE) {
      convertable_values_[i] = std::move(convertable_values[i]);
    } else {
      arg_values_[i].as_uint = arg_values[i];
      convertable_values_[i].reset();
    }
  }
  for (; i < kTraceMaxNumArgs; ++i) {
    arg_names_[i] = nullptr;
    arg_values_[i].as_uint = 0u;
    convertable_values_[i].reset();
    arg_types_[i] = TRACE_VALUE_TYPE_UINT;
  }

  bool copy = !!(flags & TRACE_EVENT_FLAG_COPY);
  size_t alloc_size = 0;
  if (copy) {
    alloc_size += GetAllocLength(name) + GetAllocLength(scope);
    for (i = 0; i < num_args; ++i) {
      alloc_size += GetAllocLength(arg_names_[i]);
      if (arg_types_[i] == TRACE_VALUE_TYPE_STRING)
        arg_types_[i] = TRACE_VALUE_TYPE_COPY_STRING;
    }
  }

  bool arg_is_copy[kTraceMaxNumArgs];
  for (i = 0; i < num_args; ++i) {
    // No copying of convertable types, we retain ownership.
    if (arg_types_[i] == TRACE_VALUE_TYPE_CONVERTABLE)
      continue;

    // We only take a copy of arg_vals if they are of type COPY_STRING.
    arg_is_copy[i] = (arg_types_[i] == TRACE_VALUE_TYPE_COPY_STRING);
    if (arg_is_copy[i])
      alloc_size += GetAllocLength(arg_values_[i].as_string);
  }

  if (alloc_size) {
    parameter_copy_storage_.reset(new std::string);
    parameter_copy_storage_->resize(alloc_size);
    char* ptr = base::data(*parameter_copy_storage_);
    const char* end = ptr + alloc_size;
    if (copy) {
      CopyTraceEventParameter(&ptr, &name_, end);
      CopyTraceEventParameter(&ptr, &scope_, end);
      for (i = 0; i < num_args; ++i) {
        CopyTraceEventParameter(&ptr, &arg_names_[i], end);
      }
    }
    for (i = 0; i < num_args; ++i) {
      if (arg_types_[i] == TRACE_VALUE_TYPE_CONVERTABLE)
        continue;
      if (arg_is_copy[i])
        CopyTraceEventParameter(&ptr, &arg_values_[i].as_string, end);
    }
    DCHECK_EQ(end, ptr) << "Overrun by " << ptr - end;
  }
}

void TraceEvent::Reset() {
  // Only reset fields that won't be initialized in Initialize(), or that may
  // hold references to other objects.
  duration_ = TimeDelta::FromInternalValue(-1);
  parameter_copy_storage_.reset();
  for (int i = 0; i < kTraceMaxNumArgs; ++i)
    convertable_values_[i].reset();
}

void TraceEvent::UpdateDuration(const TimeTicks& now,
                                const ThreadTicks& thread_now) {
  DCHECK_EQ(duration_.ToInternalValue(), -1);
  duration_ = now - timestamp_;

  // |thread_timestamp_| can be empty if the thread ticks clock wasn't
  // initialized when it was recorded.
  if (thread_timestamp_ != ThreadTicks())
    thread_duration_ = thread_now - thread_timestamp_;
}

void TraceEvent::EstimateTraceMemoryOverhead(
    TraceEventMemoryOverhead* overhead) {
  overhead->Add(TraceEventMemoryOverhead::kTraceEvent, sizeof(*this));

  if (parameter_copy_storage_)
    overhead->AddString(*parameter_copy_storage_);

  for (size_t i = 0; i < kTraceMaxNumArgs; ++i) {
    if (arg_types_[i] == TRACE_VALUE_TYPE_CONVERTABLE)
      convertable_values_[i]->EstimateTraceMemoryOverhead(overhead);
  }
}

// static
void TraceEvent::AppendValueAsJSON(unsigned char type,
                                   TraceEvent::TraceValue value,
                                   std::string* out) {
  switch (type) {
    case TRACE_VALUE_TYPE_BOOL:
      *out += value.as_bool ? "true" : "false";
      break;
    case TRACE_VALUE_TYPE_UINT:
      StringAppendF(out, "%" PRIu64, static_cast<uint64_t>(value.as_uint));
      break;
    case TRACE_VALUE_TYPE_INT:
      StringAppendF(out, "%" PRId64, static_cast<int64_t>(value.as_int));
      break;
    case TRACE_VALUE_TYPE_DOUBLE: {
      // FIXME: base/json/json_writer.cc is using the same code,
      //        should be made into a common method.
      std::string real;
      double val = value.as_double;
      if (std::isfinite(val)) {
        real = NumberToString(val);
        // Ensure that the number has a .0 if there's no decimal or 'e'.  This
        // makes sure that when we read the JSON back, it's interpreted as a
        // real rather than an int.
        if (real.find('.') == std::string::npos &&
            real.find('e') == std::string::npos &&
            real.find('E') == std::string::npos) {
          real.append(".0");
        }
        // The JSON spec requires that non-integer values in the range (-1,1)
        // have a zero before the decimal point - ".52" is not valid, "0.52" is.
        if (real[0] == '.') {
          real.insert(0, "0");
        } else if (real.length() > 1 && real[0] == '-' && real[1] == '.') {
          // "-.1" bad "-0.1" good
          real.insert(1, "0");
        }
      } else if (std::isnan(val)){
        // The JSON spec doesn't allow NaN and Infinity (since these are
        // objects in EcmaScript).  Use strings instead.
        real = "\"NaN\"";
      } else if (val < 0) {
        real = "\"-Infinity\"";
      } else {
        real = "\"Infinity\"";
      }
      StringAppendF(out, "%s", real.c_str());
      break;
    }
    case TRACE_VALUE_TYPE_POINTER:
      // JSON only supports double and int numbers.
      // So as not to lose bits from a 64-bit pointer, output as a hex string.
      StringAppendF(
          out, "\"0x%" PRIx64 "\"",
          static_cast<uint64_t>(reinterpret_cast<uintptr_t>(value.as_pointer)));
      break;
    case TRACE_VALUE_TYPE_STRING:
    case TRACE_VALUE_TYPE_COPY_STRING:
      EscapeJSONString(value.as_string ? value.as_string : "NULL", true, out);
      break;
    default:
      NOTREACHED() << "Don't know how to print this value";
      break;
  }
}

void TraceEvent::AppendAsJSON(
    std::string* out,
    const ArgumentFilterPredicate& argument_filter_predicate) const {
  int64_t time_int64 = timestamp_.ToInternalValue();
  int process_id;
  int thread_id;
  if ((flags_ & TRACE_EVENT_FLAG_HAS_PROCESS_ID) &&
      process_id_ != kNullProcessId) {
    process_id = process_id_;
    thread_id = -1;
  } else {
    process_id = TraceLog::GetInstance()->process_id();
    thread_id = thread_id_;
  }
  const char* category_group_name =
      TraceLog::GetCategoryGroupName(category_group_enabled_);

  // Category group checked at category creation time.
  DCHECK(!strchr(name_, '"'));
  StringAppendF(out, "{\"pid\":%i,\"tid\":%i,\"ts\":%" PRId64
                     ",\"ph\":\"%c\",\"cat\":\"%s\",\"name\":",
                process_id, thread_id, time_int64, phase_, category_group_name);
  EscapeJSONString(name_, true, out);
  *out += ",\"args\":";

  // Output argument names and values, stop at first NULL argument name.
  // TODO(oysteine): The dual predicates here is a bit ugly; if the filtering
  // capabilities need to grow even more precise we should rethink this
  // approach
  ArgumentNameFilterPredicate argument_name_filter_predicate;
  bool strip_args =
      arg_names_[0] && !argument_filter_predicate.is_null() &&
      !argument_filter_predicate.Run(category_group_name, name_,
                                     &argument_name_filter_predicate);

  if (strip_args) {
    *out += "\"__stripped__\"";
  } else {
    *out += "{";

    for (int i = 0; i < kTraceMaxNumArgs && arg_names_[i]; ++i) {
      if (i > 0)
        *out += ",";
      *out += "\"";
      *out += arg_names_[i];
      *out += "\":";

      if (argument_name_filter_predicate.is_null() ||
          argument_name_filter_predicate.Run(arg_names_[i])) {
        if (arg_types_[i] == TRACE_VALUE_TYPE_CONVERTABLE)
          convertable_values_[i]->AppendAsTraceFormat(out);
        else
          AppendValueAsJSON(arg_types_[i], arg_values_[i], out);
      } else {
        *out += "\"__stripped__\"";
      }
    }

    *out += "}";
  }

  if (phase_ == TRACE_EVENT_PHASE_COMPLETE) {
    int64_t duration = duration_.ToInternalValue();
    if (duration != -1)
      StringAppendF(out, ",\"dur\":%" PRId64, duration);
    if (!thread_timestamp_.is_null()) {
      int64_t thread_duration = thread_duration_.ToInternalValue();
      if (thread_duration != -1)
        StringAppendF(out, ",\"tdur\":%" PRId64, thread_duration);
    }
  }

  // Output tts if thread_timestamp is valid.
  if (!thread_timestamp_.is_null()) {
    int64_t thread_time_int64 = thread_timestamp_.ToInternalValue();
    StringAppendF(out, ",\"tts\":%" PRId64, thread_time_int64);
  }

  // Output async tts marker field if flag is set.
  if (flags_ & TRACE_EVENT_FLAG_ASYNC_TTS) {
    StringAppendF(out, ", \"use_async_tts\":1");
  }

  // If id_ is set, print it out as a hex string so we don't loose any
  // bits (it might be a 64-bit pointer).
  unsigned int id_flags_ = flags_ & (TRACE_EVENT_FLAG_HAS_ID |
                                     TRACE_EVENT_FLAG_HAS_LOCAL_ID |
                                     TRACE_EVENT_FLAG_HAS_GLOBAL_ID);
  if (id_flags_) {
    if (scope_ != trace_event_internal::kGlobalScope)
      StringAppendF(out, ",\"scope\":\"%s\"", scope_);

    switch (id_flags_) {
      case TRACE_EVENT_FLAG_HAS_ID:
        StringAppendF(out, ",\"id\":\"0x%" PRIx64 "\"",
                      static_cast<uint64_t>(id_));
        break;

      case TRACE_EVENT_FLAG_HAS_LOCAL_ID:
        StringAppendF(out, ",\"id2\":{\"local\":\"0x%" PRIx64 "\"}",
                      static_cast<uint64_t>(id_));
        break;

      case TRACE_EVENT_FLAG_HAS_GLOBAL_ID:
        StringAppendF(out, ",\"id2\":{\"global\":\"0x%" PRIx64 "\"}",
                      static_cast<uint64_t>(id_));
        break;

      default:
        NOTREACHED() << "More than one of the ID flags are set";
        break;
    }
  }

  if (flags_ & TRACE_EVENT_FLAG_BIND_TO_ENCLOSING)
    StringAppendF(out, ",\"bp\":\"e\"");

  if ((flags_ & TRACE_EVENT_FLAG_FLOW_OUT) ||
      (flags_ & TRACE_EVENT_FLAG_FLOW_IN)) {
    StringAppendF(out, ",\"bind_id\":\"0x%" PRIx64 "\"",
                  static_cast<uint64_t>(bind_id_));
  }
  if (flags_ & TRACE_EVENT_FLAG_FLOW_IN)
    StringAppendF(out, ",\"flow_in\":true");
  if (flags_ & TRACE_EVENT_FLAG_FLOW_OUT)
    StringAppendF(out, ",\"flow_out\":true");

  // Instant events also output their scope.
  if (phase_ == TRACE_EVENT_PHASE_INSTANT) {
    char scope = '?';
    switch (flags_ & TRACE_EVENT_FLAG_SCOPE_MASK) {
      case TRACE_EVENT_SCOPE_GLOBAL:
        scope = TRACE_EVENT_SCOPE_NAME_GLOBAL;
        break;

      case TRACE_EVENT_SCOPE_PROCESS:
        scope = TRACE_EVENT_SCOPE_NAME_PROCESS;
        break;

      case TRACE_EVENT_SCOPE_THREAD:
        scope = TRACE_EVENT_SCOPE_NAME_THREAD;
        break;
    }
    StringAppendF(out, ",\"s\":\"%c\"", scope);
  }

  *out += "}";
}

void TraceEvent::AppendPrettyPrinted(std::ostringstream* out) const {
  *out << name_ << "[";
  *out << TraceLog::GetCategoryGroupName(category_group_enabled_);
  *out << "]";
  if (arg_names_[0]) {
    *out << ", {";
    for (int i = 0; i < kTraceMaxNumArgs && arg_names_[i]; ++i) {
      if (i > 0)
        *out << ", ";
      *out << arg_names_[i] << ":";
      std::string value_as_text;

      if (arg_types_[i] == TRACE_VALUE_TYPE_CONVERTABLE)
        convertable_values_[i]->AppendAsTraceFormat(&value_as_text);
      else
        AppendValueAsJSON(arg_types_[i], arg_values_[i], &value_as_text);

      *out << value_as_text;
    }
    *out << "}";
  }
}

}  // namespace trace_event
}  // namespace base

namespace trace_event_internal {

std::unique_ptr<base::trace_event::ConvertableToTraceFormat>
TraceID::AsConvertableToTraceFormat() const {
  auto value = std::make_unique<base::trace_event::TracedValue>();

  if (scope_ != kGlobalScope)
    value->SetString("scope", scope_);

  const char* id_field_name = "id";
  if (id_flags_ == TRACE_EVENT_FLAG_HAS_GLOBAL_ID) {
    id_field_name = "global";
    value->BeginDictionary("id2");
  } else if (id_flags_ == TRACE_EVENT_FLAG_HAS_LOCAL_ID) {
    id_field_name = "local";
    value->BeginDictionary("id2");
  } else if (id_flags_ != TRACE_EVENT_FLAG_HAS_ID) {
    NOTREACHED() << "Unrecognized ID flag";
  }

  if (has_prefix_) {
    value->SetString(id_field_name,
                     base::StringPrintf("0x%" PRIx64 "/0x%" PRIx64,
                                        static_cast<uint64_t>(prefix_),
                                        static_cast<uint64_t>(raw_id_)));
  } else {
    value->SetString(
        id_field_name,
        base::StringPrintf("0x%" PRIx64, static_cast<uint64_t>(raw_id_)));
  }

  if (id_flags_ != TRACE_EVENT_FLAG_HAS_ID)
    value->EndDictionary();

  return std::move(value);
}

}  // namespace trace_event_internal
