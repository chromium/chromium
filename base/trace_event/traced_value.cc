// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/traced_value.h"

#include <stdint.h>

#include <utility>

#include "base/bits.h"
#include "base/containers/circular_deque.h"
#include "base/json/string_escape.h"
#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_impl.h"
#include "base/trace_event/trace_event_memory_overhead.h"
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
}  // namespace

TracedValue::TracedValue() : TracedValue(0) {}

TracedValue::TracedValue(size_t capacity) {
  DEBUG_PUSH_CONTAINER(kStackTypeDict);
  if (capacity)
    pickle_.Reserve(capacity);
}

TracedValue::~TracedValue() {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeDict);
  DEBUG_POP_CONTAINER();
  DCHECK_CONTAINER_STACK_DEPTH_EQ(0u);
}

void TracedValue::SetInteger(const char* name, int value) {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeDict);
  pickle_.WriteBytes(&kTypeInt, 1);
  pickle_.WriteInt(value);
  WriteKeyNameAsRawPtr(pickle_, name);
}

void TracedValue::SetIntegerWithCopiedName(base::StringPiece name, int value) {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeDict);
  pickle_.WriteBytes(&kTypeInt, 1);
  pickle_.WriteInt(value);
  WriteKeyNameWithCopy(pickle_, name);
}

void TracedValue::SetDouble(const char* name, double value) {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeDict);
  pickle_.WriteBytes(&kTypeDouble, 1);
  pickle_.WriteDouble(value);
  WriteKeyNameAsRawPtr(pickle_, name);
}

void TracedValue::SetDoubleWithCopiedName(base::StringPiece name,
                                          double value) {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeDict);
  pickle_.WriteBytes(&kTypeDouble, 1);
  pickle_.WriteDouble(value);
  WriteKeyNameWithCopy(pickle_, name);
}

void TracedValue::SetBoolean(const char* name, bool value) {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeDict);
  pickle_.WriteBytes(&kTypeBool, 1);
  pickle_.WriteBool(value);
  WriteKeyNameAsRawPtr(pickle_, name);
}

void TracedValue::SetBooleanWithCopiedName(base::StringPiece name, bool value) {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeDict);
  pickle_.WriteBytes(&kTypeBool, 1);
  pickle_.WriteBool(value);
  WriteKeyNameWithCopy(pickle_, name);
}

void TracedValue::SetString(const char* name, base::StringPiece value) {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeDict);
  pickle_.WriteBytes(&kTypeString, 1);
  pickle_.WriteString(value);
  WriteKeyNameAsRawPtr(pickle_, name);
}

void TracedValue::SetStringWithCopiedName(base::StringPiece name,
                                          base::StringPiece value) {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeDict);
  pickle_.WriteBytes(&kTypeString, 1);
  pickle_.WriteString(value);
  WriteKeyNameWithCopy(pickle_, name);
}

void TracedValue::SetValue(const char* name, const TracedValue& value) {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeDict);
  BeginDictionary(name);
  pickle_.WriteBytes(value.pickle_.payload(),
                     static_cast<int>(value.pickle_.payload_size()));
  EndDictionary();
}

void TracedValue::SetValueWithCopiedName(base::StringPiece name,
                                         const TracedValue& value) {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeDict);
  BeginDictionaryWithCopiedName(name);
  pickle_.WriteBytes(value.pickle_.payload(),
                     static_cast<int>(value.pickle_.payload_size()));
  EndDictionary();
}

void TracedValue::BeginDictionary(const char* name) {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeDict);
  DEBUG_PUSH_CONTAINER(kStackTypeDict);
  pickle_.WriteBytes(&kTypeStartDict, 1);
  WriteKeyNameAsRawPtr(pickle_, name);
}

void TracedValue::BeginDictionaryWithCopiedName(base::StringPiece name) {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeDict);
  DEBUG_PUSH_CONTAINER(kStackTypeDict);
  pickle_.WriteBytes(&kTypeStartDict, 1);
  WriteKeyNameWithCopy(pickle_, name);
}

void TracedValue::BeginArray(const char* name) {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeDict);
  DEBUG_PUSH_CONTAINER(kStackTypeArray);
  pickle_.WriteBytes(&kTypeStartArray, 1);
  WriteKeyNameAsRawPtr(pickle_, name);
}

void TracedValue::BeginArrayWithCopiedName(base::StringPiece name) {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeDict);
  DEBUG_PUSH_CONTAINER(kStackTypeArray);
  pickle_.WriteBytes(&kTypeStartArray, 1);
  WriteKeyNameWithCopy(pickle_, name);
}

void TracedValue::EndDictionary() {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeDict);
  DEBUG_POP_CONTAINER();
  pickle_.WriteBytes(&kTypeEndDict, 1);
}

void TracedValue::AppendInteger(int value) {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeArray);
  pickle_.WriteBytes(&kTypeInt, 1);
  pickle_.WriteInt(value);
}

void TracedValue::AppendDouble(double value) {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeArray);
  pickle_.WriteBytes(&kTypeDouble, 1);
  pickle_.WriteDouble(value);
}

void TracedValue::AppendBoolean(bool value) {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeArray);
  pickle_.WriteBytes(&kTypeBool, 1);
  pickle_.WriteBool(value);
}

void TracedValue::AppendString(base::StringPiece value) {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeArray);
  pickle_.WriteBytes(&kTypeString, 1);
  pickle_.WriteString(value);
}

void TracedValue::BeginArray() {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeArray);
  DEBUG_PUSH_CONTAINER(kStackTypeArray);
  pickle_.WriteBytes(&kTypeStartArray, 1);
}

void TracedValue::BeginDictionary() {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeArray);
  DEBUG_PUSH_CONTAINER(kStackTypeDict);
  pickle_.WriteBytes(&kTypeStartDict, 1);
}

void TracedValue::EndArray() {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeArray);
  DEBUG_POP_CONTAINER();
  pickle_.WriteBytes(&kTypeEndArray, 1);
}

void TracedValue::SetValue(const char* name,
                           std::unique_ptr<base::Value> value) {
  SetBaseValueWithCopiedName(name, *value);
}

void TracedValue::SetBaseValueWithCopiedName(base::StringPiece name,
                                             const base::Value& value) {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeDict);
  switch (value.type()) {
    case base::Value::Type::NONE:
    case base::Value::Type::BINARY:
      NOTREACHED();
      break;

    case base::Value::Type::BOOLEAN: {
      bool bool_value;
      value.GetAsBoolean(&bool_value);
      SetBooleanWithCopiedName(name, bool_value);
    } break;

    case base::Value::Type::INTEGER: {
      int int_value;
      value.GetAsInteger(&int_value);
      SetIntegerWithCopiedName(name, int_value);
    } break;

    case base::Value::Type::DOUBLE: {
      double double_value;
      value.GetAsDouble(&double_value);
      SetDoubleWithCopiedName(name, double_value);
    } break;

    case base::Value::Type::STRING: {
      const Value* string_value;
      value.GetAsString(&string_value);
      SetStringWithCopiedName(name, string_value->GetString());
    } break;

    case base::Value::Type::DICTIONARY: {
      const DictionaryValue* dict_value;
      value.GetAsDictionary(&dict_value);
      BeginDictionaryWithCopiedName(name);
      for (DictionaryValue::Iterator it(*dict_value); !it.IsAtEnd();
           it.Advance()) {
        SetBaseValueWithCopiedName(it.key(), it.value());
      }
      EndDictionary();
    } break;

    case base::Value::Type::LIST: {
      const ListValue* list_value;
      value.GetAsList(&list_value);
      BeginArrayWithCopiedName(name);
      for (const auto& base_value : *list_value)
        AppendBaseValue(base_value);
      EndArray();
    } break;
  }
}

void TracedValue::AppendBaseValue(const base::Value& value) {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeArray);
  switch (value.type()) {
    case base::Value::Type::NONE:
    case base::Value::Type::BINARY:
      NOTREACHED();
      break;

    case base::Value::Type::BOOLEAN: {
      bool bool_value;
      value.GetAsBoolean(&bool_value);
      AppendBoolean(bool_value);
    } break;

    case base::Value::Type::INTEGER: {
      int int_value;
      value.GetAsInteger(&int_value);
      AppendInteger(int_value);
    } break;

    case base::Value::Type::DOUBLE: {
      double double_value;
      value.GetAsDouble(&double_value);
      AppendDouble(double_value);
    } break;

    case base::Value::Type::STRING: {
      const Value* string_value;
      value.GetAsString(&string_value);
      AppendString(string_value->GetString());
    } break;

    case base::Value::Type::DICTIONARY: {
      const DictionaryValue* dict_value;
      value.GetAsDictionary(&dict_value);
      BeginDictionary();
      for (DictionaryValue::Iterator it(*dict_value); !it.IsAtEnd();
           it.Advance()) {
        SetBaseValueWithCopiedName(it.key(), it.value());
      }
      EndDictionary();
    } break;

    case base::Value::Type::LIST: {
      const ListValue* list_value;
      value.GetAsList(&list_value);
      BeginArray();
      for (const auto& base_value : *list_value)
        AppendBaseValue(base_value);
      EndArray();
    } break;
  }
}

std::unique_ptr<base::Value> TracedValue::ToBaseValue() const {
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
          cur_list->GetList().push_back(std::move(new_dict));
          // |new_dict| is invalidated at this point, so |cur_dict| needs to be
          // reset.
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
          cur_list->GetList().push_back(std::move(new_list));
          stack.push_back(cur_list);
          // |cur_list| is invalidated at this point by the Append, so it needs
          // to be reset.
          cur_list = &cur_list->GetList().back();
        }
      } break;

      case kTypeBool: {
        bool value;
        CHECK(it.ReadBool(&value));
        base::Value new_bool(value);
        if (cur_dict) {
          cur_dict->SetKey(ReadKeyName(it), std::move(new_bool));
        } else {
          cur_list->GetList().push_back(std::move(new_bool));
        }
      } break;

      case kTypeInt: {
        int value;
        CHECK(it.ReadInt(&value));
        base::Value new_int(value);
        if (cur_dict) {
          cur_dict->SetKey(ReadKeyName(it), std::move(new_int));
        } else {
          cur_list->GetList().push_back(std::move(new_int));
        }
      } break;

      case kTypeDouble: {
        double value;
        CHECK(it.ReadDouble(&value));
        base::Value new_double(value);
        if (cur_dict) {
          cur_dict->SetKey(ReadKeyName(it), std::move(new_double));
        } else {
          cur_list->GetList().push_back(std::move(new_double));
        }
      } break;

      case kTypeString: {
        std::string value;
        CHECK(it.ReadString(&value));
        base::Value new_str(std::move(value));
        if (cur_dict) {
          cur_dict->SetKey(ReadKeyName(it), std::move(new_str));
        } else {
          cur_list->GetList().push_back(std::move(new_str));
        }
      } break;

      default:
        NOTREACHED();
    }
  }
  DCHECK(stack.empty());
  return base::Value::ToUniquePtrValue(std::move(root));
}

void TracedValue::AppendAsTraceFormat(std::string* out) const {
  DCHECK_CURRENT_CONTAINER_IS(kStackTypeDict);
  DCHECK_CONTAINER_STACK_DEPTH_EQ(1u);

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
    if (state_stack[current_state_index].needs_comma)
      out->append(",");

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
        TraceEvent::AppendValueAsJSON(TRACE_VALUE_TYPE_BOOL, json_value, out);
        break;
      }

      case kTypeInt: {
        int value;
        CHECK(it.ReadInt(&value));
        maybe_append_key_name(state_stack[current_state_index], &it, out);
        TraceEvent::TraceValue json_value;
        json_value.as_int = value;
        TraceEvent::AppendValueAsJSON(TRACE_VALUE_TYPE_INT, json_value, out);
        break;
      }

      case kTypeDouble: {
        TraceEvent::TraceValue json_value;
        CHECK(it.ReadDouble(&json_value.as_double));
        maybe_append_key_name(state_stack[current_state_index], &it, out);
        TraceEvent::AppendValueAsJSON(TRACE_VALUE_TYPE_DOUBLE, json_value, out);
        break;
      }

      case kTypeString: {
        std::string value;
        CHECK(it.ReadString(&value));
        maybe_append_key_name(state_stack[current_state_index], &it, out);
        TraceEvent::TraceValue json_value;
        json_value.as_string = value.c_str();
        TraceEvent::AppendValueAsJSON(TRACE_VALUE_TYPE_STRING, json_value, out);
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

void TracedValue::EstimateTraceMemoryOverhead(
    TraceEventMemoryOverhead* overhead) {
  overhead->Add(TraceEventMemoryOverhead::kTracedValue,
                /* allocated size */
                pickle_.GetTotalAllocatedSize(),
                /* resident size */
                pickle_.size());
}

}  // namespace trace_event
}  // namespace base
