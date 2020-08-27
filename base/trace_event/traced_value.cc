// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/traced_value.h"

#include <inttypes.h>
#include <stdint.h>

#include <atomic>
#include <utility>

#include "base/bits.h"
#include "base/containers/circular_deque.h"
#include "base/json/json_writer.h"
#include "base/json/string_escape.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_impl.h"
#include "base/trace_event/trace_event_memory_overhead.h"
#include "base/trace_event/trace_log.h"
#include "base/values.h"

namespace base {
namespace trace_event {

namespace {
const char kTypeStartDict = '{';
const char kTypeEndDict = '}';
const char kTypeStartArray = '[';
const char kTypeEndArray = ']';
const char kTypeBool = 'b';
const char kTypeInt = 'i';
const char kTypeDouble = 'd';
const char kTypeString = 's';
const char kTypeCStr = '*';  // only used for key names

std::atomic<TracedValue::WriterFactoryCallback> g_writer_factory_callback;

#ifndef NDEBUG
const bool kStackTypeDict = false;
const bool kStackTypeArray = true;
#define DCHECK_CURRENT_CONTAINER_IS(x) DCHECK_EQ(x, nesting_stack_.back())
#define DCHECK_CONTAINER_STACK_DEPTH_EQ(x) DCHECK_EQ(x, nesting_stack_.size())
#define DEBUG_PUSH_CONTAINER(x) nesting_stack_.push_back(x)
#define DEBUG_POP_CONTAINER() nesting_stack_.pop_back()
#else
#define DCHECK_CURRENT_CONTAINER_IS(x) \
  do {                                 \
  } while (0)
#define DCHECK_CONTAINER_STACK_DEPTH_EQ(x) \
  do {                                     \
  } while (0)
#define DEBUG_PUSH_CONTAINER(x) \
  do {                          \
  } while (0)
#define DEBUG_POP_CONTAINER() \
  do {                        \
  } while (0)
#endif

inline void WriteKeyNameAsRawPtr(Pickle& pickle, const char* ptr) {
  pickle.WriteBytes(&kTypeCStr, 1);
  pickle.WriteUInt64(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ptr)));
}

inline void WriteKeyNameWithCopy(Pickle& pickle, base::StringPiece str) {
  pickle.WriteBytes(&kTypeString, 1);
  pickle.WriteString(str);
}

std::string ReadKeyName(PickleIterator& pickle_iterator) {
  const char* type = nullptr;
  bool res = pickle_iterator.ReadBytes(&type, 1);
  std::string key_name;
  if (res && *type == kTypeCStr) {
    uint64_t ptr_value = 0;
    res = pickle_iterator.ReadUInt64(&ptr_value);
    key_name = reinterpret_cast<const char*>(static_cast<uintptr_t>(ptr_value));
  } else if (res && *type == kTypeString) {
    res = pickle_iterator.ReadString(&key_name);
  }
  DCHECK(res);
  return key_name;
}

class PickleWriter final : public TracedValue::Writer {
 public:
  explicit PickleWriter(size_t capacity) {
    if (capacity) {
      pickle_.Reserve(capacity);
    }
  }

  bool IsPickleWriter() const override { return true; }
  bool IsProtoWriter() const override { return false; }

  void SetInteger(const char* name, int value) override {
    pickle_.WriteBytes(&kTypeInt, 1);
    pickle_.WriteInt(value);
    WriteKeyNameAsRawPtr(pickle_, name);
  }

  void SetIntegerWithCopiedName(base::StringPiece name, int value) override {
    pickle_.WriteBytes(&kTypeInt, 1);
    pickle_.WriteInt(value);
    WriteKeyNameWithCopy(pickle_, name);
  }

  void SetDouble(const char* name, double value) override {
    pickle_.WriteBytes(&kTypeDouble, 1);
    pickle_.WriteDouble(value);
    WriteKeyNameAsRawPtr(pickle_, name);
  }

  void SetDoubleWithCopiedName(base::StringPiece name, double value) override {
    pickle_.WriteBytes(&kTypeDouble, 1);
    pickle_.WriteDouble(value);
    WriteKeyNameWithCopy(pickle_, name);
  }

  void SetBoolean(const char* name, bool value) override {
    pickle_.WriteBytes(&kTypeBool, 1);
    pickle_.WriteBool(value);
    WriteKeyNameAsRawPtr(pickle_, name);
  }

  void SetBooleanWithCopiedName(base::StringPiece name, bool value) override {
    pickle_.WriteBytes(&kTypeBool, 1);
    pickle_.WriteBool(value);
    WriteKeyNameWithCopy(pickle_, name);
  }

  void SetString(const char* name, base::StringPiece value) override {
    pickle_.WriteBytes(&kTypeString, 1);
    pickle_.WriteString(value);
    WriteKeyNameAsRawPtr(pickle_, name);
  }

  void SetStringWithCopiedName(base::StringPiece name,
                               base::StringPiece value) override {
    pickle_.WriteBytes(&kTypeString, 1);
    pickle_.WriteString(value);
    WriteKeyNameWithCopy(pickle_, name);
  }

  void SetValue(const char* name, Writer* value) override {
    DCHECK(value->IsPickleWriter());
    const PickleWriter* pickle_writer = static_cast<const PickleWriter*>(value);

    BeginDictionary(name);
    pickle_.WriteBytes(pickle_writer->pickle_.payload(),
                       static_cast<int>(pickle_writer->pickle_.payload_size()));
    EndDictionary();
  }

  void SetValueWithCopiedName(base::StringPiece name, Writer* value) override {
    DCHECK(value->IsPickleWriter());
    const PickleWriter* pickle_writer = static_cast<const PickleWriter*>(value);

    BeginDictionaryWithCopiedName(name);
    pickle_.WriteBytes(pickle_writer->pickle_.payload(),
                       static_cast<int>(pickle_writer->pickle_.payload_size()));
    EndDictionary();
  }

  void BeginArray() override { pickle_.WriteBytes(&kTypeStartArray, 1); }

  void BeginDictionary() override { pickle_.WriteBytes(&kTypeStartDict, 1); }

  void BeginDictionary(const char* name) override {
    pickle_.WriteBytes(&kTypeStartDict, 1);
    WriteKeyNameAsRawPtr(pickle_, name);
  }

  void BeginDictionaryWithCopiedName(base::StringPiece name) override {
    pickle_.WriteBytes(&kTypeStartDict, 1);
    WriteKeyNameWithCopy(pickle_, name);
  }

  void BeginArray(const char* name) override {
    pickle_.WriteBytes(&kTypeStartArray, 1);
    WriteKeyNameAsRawPtr(pickle_, name);
  }

  void BeginArrayWithCopiedName(base::StringPiece name) override {
    pickle_.WriteBytes(&kTypeStartArray, 1);
    WriteKeyNameWithCopy(pickle_, name);
  }

  void EndDictionary() override { pickle_.WriteBytes(&kTypeEndDict, 1); }
  void EndArray() override { pickle_.WriteBytes(&kTypeEndArray, 1); }

  void AppendInteger(int value) override {
    pickle_.WriteBytes(&kTypeInt, 1);
    pickle_.WriteInt(value);
  }

  void AppendDouble(double value) override {
    pickle_.WriteBytes(&kTypeDouble, 1);
    pickle_.WriteDouble(value);
  }

  void AppendBoolean(bool value) override {
    pickle_.WriteBytes(&kTypeBool, 1);
    pickle_.WriteBool(value);
  }

  void AppendString(base::StringPiece value) override {
    pickle_.WriteBytes(&kTypeString, 1);
    pickle_.WriteString(value);
  }

  void AppendAsTraceFormat(std::string* out) const override {
    struct State {
      enum Type { kTypeDict, kTypeArray };
      Type type;
      bool needs_comma;
    };

    auto maybe_append_key_name = [](State current_state, PickleIterator* it,
                                    std::string* out) {
      if (current_state.type == State::kTypeDict) {
        EscapeJSONString(ReadKeyName(*it), true, out);
        out->append(":");
      }
    };

    base::circular_deque<State> state_stack;

    out->append("{");
    state_stack.push_back({State::kTypeDict});

    PickleIterator it(pickle_);
    for (const char* type; it.ReadBytes(&type, 1);) {
      switch (*type) {
        case kTypeEndDict:
          out->append("}");
          state_stack.pop_back();
          continue;

        case kTypeEndArray:
          out->append("]");
          state_stack.pop_back();
          continue;
      }

      // Use an index so it will stay valid across resizes.
      size_t current_state_index = state_stack.size() - 1;
      if (state_stack[current_state_index].needs_comma) {
        out->append(",");
      }

      switch (*type) {
        case kTypeStartDict: {
          maybe_append_key_name(state_stack[current_state_index], &it, out);
          out->append("{");
          state_stack.push_back({State::kTypeDict});
          break;
        }

        case kTypeStartArray: {
          maybe_append_key_name(state_stack[current_state_index], &it, out);
          out->append("[");
          state_stack.push_back({State::kTypeArray});
          break;
        }

        case kTypeBool: {
          TraceEvent::TraceValue json_value;
          CHECK(it.ReadBool(&json_value.as_bool));
          maybe_append_key_name(state_stack[current_state_index], &it, out);
          json_value.AppendAsJSON(TRACE_VALUE_TYPE_BOOL, out);
          break;
        }

        case kTypeInt: {
          int value;
          CHECK(it.ReadInt(&value));
          maybe_append_key_name(state_stack[current_state_index], &it, out);
          TraceEvent::TraceValue json_value;
          json_value.as_int = value;
          json_value.AppendAsJSON(TRACE_VALUE_TYPE_INT, out);
          break;
        }

        case kTypeDouble: {
          TraceEvent::TraceValue json_value;
          CHECK(it.ReadDouble(&json_value.as_double));
          maybe_append_key_name(state_stack[current_state_index], &it, out);
          json_value.AppendAsJSON(TRACE_VALUE_TYPE_DOUBLE, out);
          break;
        }

        case kTypeString: {
          std::string value;
          CHECK(it.ReadString(&value));
          maybe_append_key_name(state_stack[current_state_index], &it, out);
          TraceEvent::TraceValue json_value;
          json_value.as_string = value.c_str();
          json_value.AppendAsJSON(TRACE_VALUE_TYPE_STRING, out);
          break;
        }

        default:
          NOTREACHED();
      }

      state_stack[current_state_index].needs_comma = true;
    }

    out->append("}");
    state_stack.pop_back();

    DCHECK(state_stack.empty());
  }

  void EstimateTraceMemoryOverhead(
      TraceEventMemoryOverhead* overhead) override {
    overhead->Add(TraceEventMemoryOverhead::kTracedValue,
                  /* allocated size */
                  pickle_.GetTotalAllocatedSize(),
                  /* resident size */
                  pickle_.size());
  }

  std::unique_ptr<base::Value> ToBaseValue() const {
    base::Value root(base::Value::Type::DICTIONARY);
    Value* cur_dict = &root;
    Value* cur_list = nullptr;
    std::vector<Value*> stack;
    PickleIterator it(pickle_);
    const char* type;

    while (it.ReadBytes(&type, 1)) {
      DCHECK((cur_dict && !cur_list) || (cur_list && !cur_dict));
      switch (*type) {
        case kTypeStartDict: {
          base::Value new_dict(base::Value::Type::DICTIONARY);
          if (cur_dict) {
            stack.push_back(cur_dict);
            cur_dict = cur_dict->SetKey(ReadKeyName(it), std::move(new_dict));
          } else {
            cur_list->Append(std::move(new_dict));
            // |new_dict| is invalidated at this point, so |cur_dict| needs to
            // be reset.
            cur_dict = &cur_list->GetList().back();
            stack.push_back(cur_list);
            cur_list = nullptr;
          }
        } break;

        case kTypeEndArray:
        case kTypeEndDict: {
          if (stack.back()->is_dict()) {
            cur_dict = stack.back();
            cur_list = nullptr;
          } else if (stack.back()->is_list()) {
            cur_list = stack.back();
            cur_dict = nullptr;
          }
          stack.pop_back();
        } break;

        case kTypeStartArray: {
          base::Value new_list(base::Value::Type::LIST);
          if (cur_dict) {
            stack.push_back(cur_dict);
            cur_list = cur_dict->SetKey(ReadKeyName(it), std::move(new_list));
            cur_dict = nullptr;
          } else {
            cur_list->Append(std::move(new_list));
            stack.push_back(cur_list);
            // |cur_list| is invalidated at this point by the Append, so it
            // needs to be reset.
            cur_list = &cur_list->GetList().back();
          }
        } break;

        case kTypeBool: {
          bool value;
          CHECK(it.ReadBool(&value));
          if (cur_dict) {
            cur_dict->SetBoolKey(ReadKeyName(it), value);
          } else {
            cur_list->Append(value);
          }
        } break;

        case kTypeInt: {
          int value;
          CHECK(it.ReadInt(&value));
          if (cur_dict) {
            cur_dict->SetIntKey(ReadKeyName(it), value);
          } else {
            cur_list->Append(value);
          }
        } break;

        case kTypeDouble: {
          TraceEvent::TraceValue trace_value;
          CHECK(it.ReadDouble(&trace_value.as_double));
          Value base_value;
          if (!std::isfinite(trace_value.as_double)) {
            // base::Value doesn't support nan and infinity values. Use strings
            // for them instead. This follows the same convention in
            // AppendAsTraceFormat(), supported by TraceValue::Append*().
            std::string value_string;
            trace_value.AppendAsString(TRACE_VALUE_TYPE_DOUBLE, &value_string);
            base_value = Value(value_string);
          } else {
            base_value = Value(trace_value.as_double);
          }
          if (cur_dict) {
            cur_dict->SetKey(ReadKeyName(it), std::move(base_value));
          } else {
            cur_list->Append(std::move(base_value));
          }
        } break;

        case kTypeString: {
          std::string value;
          CHECK(it.ReadString(&value));
          if (cur_dict) {
            cur_dict->SetStringKey(ReadKeyName(it), std::move(value));
          } else {
            cur_list->Append(std::move(value));
          }
        } break;

        default:
          NOTREACHED();
      }
    }
    DCHECK(stack.empty());
    return base::Value::ToUniquePtrValue(std::move(root));
  }

 private:
  Pickle pickle_;
};

std::unique_ptr<TracedValue::Writer> CreateWriter(size_t capacity) {
  TracedValue::WriterFactoryCallback callback =
      g_writer_factory_callback.load(std::memory_order_relaxed);
  if (callback) {
    return callback(capacity);
  }

  return std::make_unique<PickleWriter>(capacity);
}

}  // namespace

bool TracedValue::Writer::AppendToProto(ProtoAppender* appender) {
  return false;
}

// static
void TracedValue::SetWriterFactoryCallback(WriterFactoryCallback callback) {
  g_writer_factory_callback.store(callback);
}

TracedValue::TracedValue(size_t capacity)
    : TracedValue(capacity, /*forced_json*/ false) {}

TracedValue::TracedValue(size_t capacity, bool forced_json) {
  DEBUG_PUSH_CONTAINER(kStackTypeDict);

  writer_ = forced_json ? std::make_unique<PickleWriter>(capacity)
                        : CreateWriter(capacity);
}

TracedValue::~TracedValue() {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeDict);
  DEBUG_POP_CONTAINER();
  DCHECK_CONTAINER_STACK_DEPTH_EQ(0u);
}

void TracedValue::SetInteger(const char* name, int value) {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeDict);
  writer_->SetInteger(name, value);
}

void TracedValue::SetIntegerWithCopiedName(base::StringPiece name, int value) {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeDict);
  writer_->SetIntegerWithCopiedName(name, value);
}

void TracedValue::SetDouble(const char* name, double value) {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeDict);
  writer_->SetDouble(name, value);
}

void TracedValue::SetDoubleWithCopiedName(base::StringPiece name,
                                          double value) {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeDict);
  writer_->SetDoubleWithCopiedName(name, value);
}

void TracedValue::SetBoolean(const char* name, bool value) {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeDict);
  writer_->SetBoolean(name, value);
}

void TracedValue::SetBooleanWithCopiedName(base::StringPiece name, bool value) {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeDict);
  writer_->SetBooleanWithCopiedName(name, value);
}

void TracedValue::SetString(const char* name, base::StringPiece value) {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeDict);
  writer_->SetString(name, value);
}

void TracedValue::SetStringWithCopiedName(base::StringPiece name,
                                          base::StringPiece value) {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeDict);
  writer_->SetStringWithCopiedName(name, value);
}

void TracedValue::SetValue(const char* name, TracedValue* value) {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeDict);
  writer_->SetValue(name, value->writer_.get());
}

void TracedValue::SetValueWithCopiedName(base::StringPiece name,
                                         TracedValue* value) {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeDict);
  writer_->SetValueWithCopiedName(name, value->writer_.get());
}

namespace {

// TODO(altimin): Add native support for pointers for nested values in
// DebugAnnotation proto.
std::string PointerToString(void* value) {
  return base::StringPrintf(
      "0x%" PRIx64, static_cast<uint64_t>(reinterpret_cast<uintptr_t>(value)));
}

}  // namespace

void TracedValue::SetPointer(const char* name, void* value) {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeDict);
  writer_->SetString(name, PointerToString(value));
}

void TracedValue::SetPointerWithCopiedName(base::StringPiece name,
                                           void* value) {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeDict);
  writer_->SetStringWithCopiedName(name, PointerToString(value));
}

void TracedValue::BeginDictionary(const char* name) {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeDict);
  DEBUG_PUSH_CONTAINER(kStackTypeDict);
  writer_->BeginDictionary(name);
}

void TracedValue::BeginDictionaryWithCopiedName(base::StringPiece name) {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeDict);
  DEBUG_PUSH_CONTAINER(kStackTypeDict);
  writer_->BeginDictionaryWithCopiedName(name);
}

void TracedValue::BeginArray(const char* name) {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeDict);
  DEBUG_PUSH_CONTAINER(kStackTypeArray);
  writer_->BeginArray(name);
}

void TracedValue::BeginArrayWithCopiedName(base::StringPiece name) {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeDict);
  DEBUG_PUSH_CONTAINER(kStackTypeArray);
  writer_->BeginArrayWithCopiedName(name);
}

void TracedValue::AppendInteger(int value) {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeArray);
  writer_->AppendInteger(value);
}

void TracedValue::AppendDouble(double value) {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeArray);
  writer_->AppendDouble(value);
}

void TracedValue::AppendBoolean(bool value) {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeArray);
  writer_->AppendBoolean(value);
}

void TracedValue::AppendString(base::StringPiece value) {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeArray);
  writer_->AppendString(value);
}

void TracedValue::AppendPointer(void* value) {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeArray);
  writer_->AppendString(PointerToString(value));
}

void TracedValue::BeginArray() {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeArray);
  DEBUG_PUSH_CONTAINER(kStackTypeArray);
  writer_->BeginArray();
}

void TracedValue::BeginDictionary() {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeArray);
  DEBUG_PUSH_CONTAINER(kStackTypeDict);
  writer_->BeginDictionary();
}

void TracedValue::EndArray() {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeArray);
  DEBUG_POP_CONTAINER();
  writer_->EndArray();
}

void TracedValue::EndDictionary() {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeDict);
  DEBUG_POP_CONTAINER();
  writer_->EndDictionary();
}

std::unique_ptr<base::Value> TracedValue::ToBaseValue() const {
  DCHECK(writer_->IsPickleWriter());
  return static_cast<const PickleWriter*>(writer_.get())->ToBaseValue();
}

void TracedValue::AppendAsTraceFormat(std::string* out) const {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeDict);
  DCHECK_CONTAINER_STACK_DEPTH_EQ(1u);

  writer_->AppendAsTraceFormat(out);
}

bool TracedValue::AppendToProto(ProtoAppender* appender) {
  return writer_->AppendToProto(appender);
}

void TracedValue::EstimateTraceMemoryOverhead(
    TraceEventMemoryOverhead* overhead) {
  writer_->EstimateTraceMemoryOverhead(overhead);
}

TracedValue::Array::Array(const std::initializer_list<ArrayItem> items) {
  items_ = std::move(items);
}

TracedValue::Array::Array(TracedValue::Array&& other) {
  items_ = std::move(other.items_);
}

void TracedValue::Array::WriteToValue(TracedValue* value) const {
  for (const auto& item : items_) {
    item.WriteToValue(value);
  }
}

TracedValue::Dictionary::Dictionary(
    const std::initializer_list<DictionaryItem> items) {
  items_ = items;
}

TracedValue::Dictionary::Dictionary(TracedValue::Dictionary&& other) {
  items_ = std::move(other.items_);
}

void TracedValue::Dictionary::WriteToValue(TracedValue* value) const {
  for (const auto& item : items_) {
    item.WriteToValue(value);
  }
}

TracedValue::ValueHolder::ValueHolder(int value) {
  kept_value_.int_value = value;
  kept_value_type_ = KeptValueType::kIntType;
}

TracedValue::ValueHolder::ValueHolder(double value) {
  kept_value_.double_value = value;
  kept_value_type_ = KeptValueType::kDoubleType;
}

TracedValue::ValueHolder::ValueHolder(bool value) {
  kept_value_.bool_value = value;
  kept_value_type_ = KeptValueType::kBoolType;
}

TracedValue::ValueHolder::ValueHolder(base::StringPiece value) {
  kept_value_.string_piece_value = value;
  kept_value_type_ = KeptValueType::kStringPieceType;
}

TracedValue::ValueHolder::ValueHolder(std::string value) {
  new (&kept_value_.std_string_value) std::string(std::move(value));
  kept_value_type_ = KeptValueType::kStdStringType;
}

TracedValue::ValueHolder::ValueHolder(void* value) {
  kept_value_.void_ptr_value = value;
  kept_value_type_ = KeptValueType::kVoidPtrType;
}

TracedValue::ValueHolder::ValueHolder(const char* value) {
  kept_value_.string_piece_value = value;
  kept_value_type_ = KeptValueType::kStringPieceType;
}

TracedValue::ValueHolder::ValueHolder(TracedValue::Dictionary& value) {
  new (&kept_value_.dictionary_value) TracedValue::Dictionary(std::move(value));
  kept_value_type_ = KeptValueType::kDictionaryType;
}

TracedValue::ValueHolder::ValueHolder(TracedValue::Array& value) {
  new (&kept_value_.array_value) TracedValue::Array(std::move(value));
  kept_value_type_ = KeptValueType::kArrayType;
}

TracedValue::ValueHolder::ValueHolder(TracedValue::ValueHolder&& other) {
  // Remember to call a destructor if necessary.
  if (kept_value_type_ == KeptValueType::kStdStringType) {
    delete (&kept_value_.std_string_value);
  }
  switch (other.kept_value_type_) {
    case KeptValueType::kIntType: {
      kept_value_.int_value = other.kept_value_.int_value;
      break;
    }
    case KeptValueType::kDoubleType: {
      kept_value_.double_value = other.kept_value_.double_value;
      break;
    }
    case KeptValueType::kBoolType: {
      kept_value_.bool_value = other.kept_value_.bool_value;
      break;
    }
    case KeptValueType::kStringPieceType: {
      kept_value_.string_piece_value = other.kept_value_.string_piece_value;
      break;
    }
    case KeptValueType::kStdStringType: {
      new (&kept_value_.std_string_value)
          std::string(std::move(other.kept_value_.std_string_value));
      break;
    }
    case KeptValueType::kVoidPtrType: {
      kept_value_.void_ptr_value = other.kept_value_.void_ptr_value;
      break;
    }
    case KeptValueType::kArrayType: {
      new (&kept_value_.array_value)
          TracedValue::Array(std::move(other.kept_value_.array_value));
      break;
    }
    case KeptValueType::kDictionaryType: {
      new (&kept_value_.dictionary_value) TracedValue::Dictionary(
          std::move(other.kept_value_.dictionary_value));
      break;
    }
  }
  kept_value_type_ = other.kept_value_type_;
}

void TracedValue::ValueHolder::WriteToValue(TracedValue* value) const {
  switch (kept_value_type_) {
    case KeptValueType::kIntType: {
      value->AppendInteger(kept_value_.int_value);
      break;
    }
    case KeptValueType::kDoubleType: {
      value->AppendDouble(kept_value_.double_value);
      break;
    }
    case KeptValueType::kBoolType: {
      value->AppendBoolean(kept_value_.bool_value);
      break;
    }
    case KeptValueType::kStringPieceType: {
      value->AppendString(kept_value_.string_piece_value);
      break;
    }
    case KeptValueType::kStdStringType: {
      value->AppendString(kept_value_.std_string_value);
      break;
    }
    case KeptValueType::kVoidPtrType: {
      value->AppendPointer(kept_value_.void_ptr_value);
      break;
    }
    case KeptValueType::kArrayType: {
      value->BeginArray();
      kept_value_.array_value.WriteToValue(value);
      value->EndArray();
      break;
    }
    case KeptValueType::kDictionaryType: {
      value->BeginDictionary();
      kept_value_.dictionary_value.WriteToValue(value);
      value->EndDictionary();
      break;
    }
  }
}

void TracedValue::ValueHolder::WriteToValue(const char* name,
                                            TracedValue* value) const {
  switch (kept_value_type_) {
    case KeptValueType::kIntType: {
      value->SetInteger(name, kept_value_.int_value);
      break;
    }
    case KeptValueType::kDoubleType: {
      value->SetDouble(name, kept_value_.double_value);
      break;
    }
    case KeptValueType::kBoolType: {
      value->SetBoolean(name, kept_value_.bool_value);
      break;
    }
    case KeptValueType::kStringPieceType: {
      value->SetString(name, kept_value_.string_piece_value);
      break;
    }
    case KeptValueType::kStdStringType: {
      value->SetString(name, kept_value_.std_string_value);
      break;
    }
    case KeptValueType::kVoidPtrType: {
      value->SetPointer(name, kept_value_.void_ptr_value);
      break;
    }
    case KeptValueType::kArrayType: {
      value->BeginArray(name);
      kept_value_.array_value.WriteToValue(value);
      value->EndArray();
      break;
    }
    case KeptValueType::kDictionaryType: {
      value->BeginDictionary(name);
      kept_value_.dictionary_value.WriteToValue(value);
      value->EndDictionary();
      break;
    }
  }
}

void TracedValue::ArrayItem::WriteToValue(TracedValue* value) const {
  ValueHolder::WriteToValue(value);
}

void TracedValue::DictionaryItem::WriteToValue(TracedValue* value) const {
  ValueHolder::WriteToValue(name_, value);
}

std::unique_ptr<TracedValue> TracedValue::Build(
    const std::initializer_list<DictionaryItem> items) {
  std::unique_ptr<TracedValue> value(new TracedValue());
  for (const auto& item : items) {
    item.WriteToValue(value.get());
  }
  return value;
}

std::string TracedValueJSON::ToJSON() const {
  std::string result;
  AppendAsTraceFormat(&result);
  return result;
}

std::string TracedValueJSON::ToFormattedJSON() const {
  std::string str;
  base::JSONWriter::WriteWithOptions(
      *ToBaseValue(),
      base::JSONWriter::OPTIONS_OMIT_DOUBLE_TYPE_PRESERVATION |
          base::JSONWriter::OPTIONS_PRETTY_PRINT,
      &str);
  return str;
}

TracedValue::ArrayScope::ArrayScope(TracedValue* value) : value_(value) {}

TracedValue::ArrayScope::~ArrayScope() {
  value_->EndArray();
}

TracedValue::ArrayScope TracedValue::AppendArrayScoped() {
  BeginArray();
  return TracedValue::ArrayScope(this);
}

TracedValue::ArrayScope TracedValue::BeginArrayScoped(const char* name) {
  BeginArray(name);
  return TracedValue::ArrayScope(this);
}

TracedValue::ArrayScope TracedValue::BeginArrayScopedWithCopiedName(
    base::StringPiece name) {
  BeginArrayWithCopiedName(name);
  return TracedValue::ArrayScope(this);
}

TracedValue::DictionaryScope::DictionaryScope(TracedValue* value)
    : value_(value) {}

TracedValue::DictionaryScope::~DictionaryScope() {
  value_->EndDictionary();
}

TracedValue::DictionaryScope TracedValue::AppendDictionaryScoped() {
  BeginDictionary();
  return TracedValue::DictionaryScope(this);
}

TracedValue::DictionaryScope TracedValue::BeginDictionaryScoped(
    const char* name) {
  BeginDictionary(name);
  return TracedValue::DictionaryScope(this);
}

TracedValue::DictionaryScope TracedValue::BeginDictionaryScopedWithCopiedName(
    base::StringPiece name) {
  BeginDictionaryWithCopiedName(name);
  return TracedValue::DictionaryScope(this);
}

}  // namespace trace_event
}  // namespace base
