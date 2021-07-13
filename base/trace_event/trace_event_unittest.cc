// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/trace_event.h"

#include <inttypes.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include <cstdlib>
#include <limits>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/cxx20_erase_vector.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/singleton.h"
#include "base/process/process_handle.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/pattern.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "base/trace_event/event_name_filter.h"
#include "base/trace_event/trace_buffer.h"
#include "base/trace_event/trace_event_filter.h"
#include "base/trace_event/trace_event_filter_test_utils.h"
#include "base/values.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace trace_event {

namespace {

enum CompareOp {
  IS_EQUAL,
  IS_NOT_EQUAL,
};

struct JsonKeyValue {
  const char* key;
  const char* value;
  CompareOp op;
};

const int kThreadId = 42;
const int kAsyncId = 5;
const char kAsyncIdStr[] = "0x5";
const int kAsyncId2 = 6;
const char kAsyncId2Str[] = "0x6";

constexpr const char kRecordAllCategoryFilter[] = "*";
constexpr const char kAllCategory[] = "test_all";

bool IsCategoryEnabled(const char* name) {
#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  bool result;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(perfetto::DynamicCategory(name), &result);
  return result;
#else
  return *TraceLog::GetInstance()->GetCategoryGroupEnabled(name);
#endif
}

class TraceEventTestFixture : public testing::Test {
 public:
  TraceEventTestFixture() : trace_parsed_(Value::Type::LIST) {}

  void OnTraceDataCollected(
      WaitableEvent* flush_complete_event,
      const scoped_refptr<base::RefCountedString>& events_str,
      bool has_more_events);
  const Value* FindMatchingTraceEntry(const JsonKeyValue* key_values);
  const Value* FindNamePhase(const char* name, const char* phase);
  const Value* FindNamePhaseKeyValue(const char* name,
                                     const char* phase,
                                     const char* key,
                                     const char* value);
  void DropTracedMetadataRecords();
  bool FindMatchingValue(const char* key,
                         const char* value);
  bool FindNonMatchingValue(const char* key,
                            const char* value);
  void Clear() {
    trace_parsed_ = Value(Value::Type::LIST);
    json_output_.json_output.clear();
  }

  void BeginTrace() {
    BeginSpecificTrace("*");
  }

  void BeginSpecificTrace(const std::string& filter) {
    TraceLog::GetInstance()->SetEnabled(TraceConfig(filter, ""),
                                        TraceLog::RECORDING_MODE);
  }

  void CancelTrace() {
    WaitableEvent flush_complete_event(
        WaitableEvent::ResetPolicy::AUTOMATIC,
        WaitableEvent::InitialState::NOT_SIGNALED);
    CancelTraceAsync(&flush_complete_event);
    flush_complete_event.Wait();
  }

  void EndTraceAndFlush() {
    num_flush_callbacks_ = 0;
    WaitableEvent flush_complete_event(
        WaitableEvent::ResetPolicy::AUTOMATIC,
        WaitableEvent::InitialState::NOT_SIGNALED);
    EndTraceAndFlushAsync(&flush_complete_event);
    flush_complete_event.Wait();
  }

  // Used when testing thread-local buffers which requires the thread initiating
  // flush to have a message loop.
  void EndTraceAndFlushInThreadWithMessageLoop() {
    WaitableEvent flush_complete_event(
        WaitableEvent::ResetPolicy::AUTOMATIC,
        WaitableEvent::InitialState::NOT_SIGNALED);
    Thread flush_thread("flush");
    flush_thread.Start();
    flush_thread.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&TraceEventTestFixture::EndTraceAndFlushAsync,
                       base::Unretained(this), &flush_complete_event));
    flush_complete_event.Wait();
  }

  void CancelTraceAsync(WaitableEvent* flush_complete_event) {
    TraceLog::GetInstance()->CancelTracing(base::BindRepeating(
        &TraceEventTestFixture::OnTraceDataCollected,
        base::Unretained(static_cast<TraceEventTestFixture*>(this)),
        base::Unretained(flush_complete_event)));
  }

  void EndTraceAndFlushAsync(WaitableEvent* flush_complete_event) {
    TraceLog::GetInstance()->SetDisabled(TraceLog::RECORDING_MODE |
                                         TraceLog::FILTERING_MODE);
    TraceLog::GetInstance()->Flush(base::BindRepeating(
        &TraceEventTestFixture::OnTraceDataCollected,
        base::Unretained(static_cast<TraceEventTestFixture*>(this)),
        base::Unretained(flush_complete_event)));
  }

  void SetUp() override {
    const char* name = PlatformThread::GetName();
    old_thread_name_ = name ? strdup(name) : nullptr;

    TraceLog::ResetForTesting();
    TraceLog* tracelog = TraceLog::GetInstance();
    ASSERT_TRUE(tracelog);
    ASSERT_FALSE(tracelog->IsEnabled());
    trace_buffer_.SetOutputCallback(json_output_.GetCallback());
    num_flush_callbacks_ = 0;
  }
  void TearDown() override {
    if (TraceLog::GetInstance())
      EXPECT_FALSE(TraceLog::GetInstance()->IsEnabled());
    PlatformThread::SetName(old_thread_name_ ? old_thread_name_ : "");
    free(old_thread_name_);
    old_thread_name_ = nullptr;
    // We want our singleton torn down after each test.
    TraceLog::ResetForTesting();
  }

  char* old_thread_name_;
  Value trace_parsed_;
  TraceResultBuffer trace_buffer_;
  TraceResultBuffer::SimpleOutput json_output_;
  size_t num_flush_callbacks_;

 private:
  // We want our singleton torn down after each test.
  ShadowingAtExitManager at_exit_manager_;
  Lock lock_;
};

void TraceEventTestFixture::OnTraceDataCollected(
    WaitableEvent* flush_complete_event,
    const scoped_refptr<base::RefCountedString>& events_str,
    bool has_more_events) {
  num_flush_callbacks_++;
  if (num_flush_callbacks_ > 1) {
    EXPECT_FALSE(events_str->data().empty());
  }
  AutoLock lock(lock_);
  json_output_.json_output.clear();
  trace_buffer_.Start();
  trace_buffer_.AddFragment(events_str->data());
  trace_buffer_.Finish();

  absl::optional<Value> root =
      base::JSONReader::Read(json_output_.json_output, JSON_PARSE_RFC);

  if (!root.has_value()) {
    LOG(ERROR) << json_output_.json_output;
  }

  ASSERT_TRUE(root->is_list());
  Value::ListStorage root_storage = std::move(*root).TakeList();

  // Move items into our aggregate collection
  Value::ListStorage storage = std::move(trace_parsed_).TakeList();
  storage.reserve(storage.size() + root_storage.size());
  std::move(root_storage.begin(), root_storage.end(),
            std::back_inserter(storage));
  trace_parsed_ = Value(std::move(storage));

  if (!has_more_events)
    flush_complete_event->Signal();
}

static bool CompareJsonValues(const std::string& lhs,
                              const std::string& rhs,
                              CompareOp op) {
  switch (op) {
    case IS_EQUAL:
      return lhs == rhs;
    case IS_NOT_EQUAL:
      return lhs != rhs;
    default:
      CHECK(0);
  }
  return false;
}

static bool IsKeyValueInDict(const JsonKeyValue* key_value, const Value* dict) {
  const std::string* value_str = dict->FindStringPath(key_value->key);
  if (value_str &&
      CompareJsonValues(*value_str, key_value->value, key_value->op))
    return true;

  // Recurse to test arguments
  const Value* args_dict = dict->FindDictPath("args");
  if (args_dict)
    return IsKeyValueInDict(key_value, args_dict);

  return false;
}

static bool IsAllKeyValueInDict(const JsonKeyValue* key_values,
                                const Value* dict) {
  // Scan all key_values, they must all be present and equal.
  while (key_values && key_values->key) {
    if (!IsKeyValueInDict(key_values, dict))
      return false;
    ++key_values;
  }
  return true;
}

const Value* TraceEventTestFixture::FindMatchingTraceEntry(
    const JsonKeyValue* key_values) {
  // Scan all items
  for (const Value& value : trace_parsed_.GetList()) {
    if (!value.is_dict())
      continue;

    if (IsAllKeyValueInDict(key_values, &value))
      return &value;
  }
  return nullptr;
}

void TraceEventTestFixture::DropTracedMetadataRecords() {
  Value::ListStorage storage = std::move(trace_parsed_).TakeList();
  base::EraseIf(storage, [](const Value& value) {
    if (!value.is_dict())
      return false;
    const std::string* ph = value.FindStringKey("ph");
    return ph && *ph == "M";
  });
  trace_parsed_ = Value(std::move(storage));
}

const Value* TraceEventTestFixture::FindNamePhase(const char* name,
                                                  const char* phase) {
  JsonKeyValue key_values[] = {{"name", name, IS_EQUAL},
                               {"ph", phase, IS_EQUAL},
                               {nullptr, nullptr, IS_EQUAL}};
  return FindMatchingTraceEntry(key_values);
}

const Value* TraceEventTestFixture::FindNamePhaseKeyValue(const char* name,
                                                          const char* phase,
                                                          const char* key,
                                                          const char* value) {
  JsonKeyValue key_values[] = {{"name", name, IS_EQUAL},
                               {"ph", phase, IS_EQUAL},
                               {key, value, IS_EQUAL},
                               {nullptr, nullptr, IS_EQUAL}};
  return FindMatchingTraceEntry(key_values);
}

bool TraceEventTestFixture::FindMatchingValue(const char* key,
                                              const char* value) {
  JsonKeyValue key_values[] = {{key, value, IS_EQUAL},
                               {nullptr, nullptr, IS_EQUAL}};
  return FindMatchingTraceEntry(key_values);
}

bool TraceEventTestFixture::FindNonMatchingValue(const char* key,
                                                 const char* value) {
  JsonKeyValue key_values[] = {{key, value, IS_NOT_EQUAL},
                               {nullptr, nullptr, IS_EQUAL}};
  return FindMatchingTraceEntry(key_values);
}

bool IsStringInDict(const char* string_to_match, const Value* dict) {
  for (const auto pair : dict->DictItems()) {
    if (pair.first.find(string_to_match) != std::string::npos)
      return true;

    if (!pair.second.is_string())
      continue;

    if (pair.second.GetString().find(string_to_match) != std::string::npos)
      return true;
  }

  // Recurse to test arguments
  const Value* args_dict = dict->FindDictKey("args");
  if (args_dict)
    return IsStringInDict(string_to_match, args_dict);

  return false;
}

const Value* FindTraceEntry(const Value& trace_parsed,
                            const char* string_to_match,
                            const Value* match_after_this_item = nullptr) {
  // Scan all items
  for (const Value& value : trace_parsed.GetList()) {
    if (match_after_this_item) {
      if (&value == match_after_this_item)
        match_after_this_item = nullptr;
      continue;
    }
    if (!value.is_dict())
      continue;

    if (IsStringInDict(string_to_match, &value))
      return &value;
  }
  return nullptr;
}

std::vector<const Value*> FindTraceEntries(const Value& trace_parsed,
                                           const char* string_to_match) {
  std::vector<const Value*> hits;
  for (const Value& value : trace_parsed.GetList()) {
    if (!value.is_dict())
      continue;

    if (IsStringInDict(string_to_match, &value))
      hits.push_back(&value);
  }
  return hits;
}

constexpr const char kControlCharacters[] = "test_\001\002\003\n\r";

void TraceWithAllMacroVariants(WaitableEvent* task_complete_event) {
  {
    TRACE_EVENT0("test_all", "TRACE_EVENT0 call");
    TRACE_EVENT1("test_all", "TRACE_EVENT1 call", "name1", "value1");
    TRACE_EVENT2("test_all", "TRACE_EVENT2 call", "name1", "\"value1\"",
                 "name2", "value\\2");

    TRACE_EVENT_INSTANT0("test_all", "TRACE_EVENT_INSTANT0 call",
                         TRACE_EVENT_SCOPE_GLOBAL);
    TRACE_EVENT_INSTANT1("test_all", "TRACE_EVENT_INSTANT1 call",
                         TRACE_EVENT_SCOPE_PROCESS, "name1", "value1");
    TRACE_EVENT_INSTANT2("test_all", "TRACE_EVENT_INSTANT2 call",
                         TRACE_EVENT_SCOPE_THREAD, "name1", "value1", "name2",
                         "value2");

    TRACE_EVENT_BEGIN0("test_all", "TRACE_EVENT_BEGIN0 call");
    TRACE_EVENT_BEGIN1("test_all", "TRACE_EVENT_BEGIN1 call", "name1",
                       "value1");
    TRACE_EVENT_BEGIN2("test_all", "TRACE_EVENT_BEGIN2 call", "name1", "value1",
                       "name2", "value2");

    TRACE_EVENT_END0("test_all", "TRACE_EVENT_BEGIN2 call");
    TRACE_EVENT_END1("test_all", "TRACE_EVENT_BEGIN1 call", "name1", "value1");
    TRACE_EVENT_END2("test_all", "TRACE_EVENT_BEGIN0 call", "name1", "value1",
                     "name2", "value2");

    TRACE_EVENT_ASYNC_BEGIN0("test_all", "TRACE_EVENT_ASYNC_BEGIN0 call",
                             kAsyncId);
    TRACE_EVENT_ASYNC_BEGIN1("test_all", "TRACE_EVENT_ASYNC_BEGIN1 call",
                             kAsyncId, "name1", "value1");
    TRACE_EVENT_ASYNC_BEGIN2("test_all", "TRACE_EVENT_ASYNC_BEGIN2 call",
                             kAsyncId, "name1", "value1", "name2", "value2");

    TRACE_EVENT_ASYNC_STEP_INTO0("test_all",
                                 "TRACE_EVENT_ASYNC_STEP_INTO0 call", kAsyncId,
                                 "step_begin1");
    TRACE_EVENT_ASYNC_STEP_INTO1("test_all",
                                 "TRACE_EVENT_ASYNC_STEP_INTO1 call", kAsyncId,
                                 "step_begin2", "name1", "value1");

    TRACE_EVENT_ASYNC_END0("test_all", "TRACE_EVENT_ASYNC_END0 call", kAsyncId);
    TRACE_EVENT_ASYNC_END1("test_all", "TRACE_EVENT_ASYNC_END1 call", kAsyncId,
                           "name1", "value1");
    TRACE_EVENT_ASYNC_END2("test_all", "TRACE_EVENT_ASYNC_END2 call", kAsyncId,
                           "name1", "value1", "name2", "value2");

    TRACE_COUNTER1("test_all", "TRACE_COUNTER1 call", 31415);
    TRACE_COUNTER2("test_all", "TRACE_COUNTER2 call", "a", 30000, "b", 1415);

    TRACE_COUNTER_WITH_TIMESTAMP1("test_all",
                                  "TRACE_COUNTER_WITH_TIMESTAMP1 call",
                                  TimeTicks::FromInternalValue(42), 31415);
    TRACE_COUNTER_WITH_TIMESTAMP2(
        "test_all", "TRACE_COUNTER_WITH_TIMESTAMP2 call",
        TimeTicks::FromInternalValue(42), "a", 30000, "b", 1415);

    TRACE_COUNTER_ID1("test_all", "TRACE_COUNTER_ID1 call", 0x319009, 31415);
    TRACE_COUNTER_ID2("test_all", "TRACE_COUNTER_ID2 call", 0x319009, "a",
                      30000, "b", 1415);

    TRACE_EVENT_COPY_BEGIN_WITH_ID_TID_AND_TIMESTAMP0(
        "test_all", "TRACE_EVENT_COPY_BEGIN_WITH_ID_TID_AND_TIMESTAMP0 call",
        kAsyncId, kThreadId, TimeTicks::FromInternalValue(12345));
    TRACE_EVENT_COPY_END_WITH_ID_TID_AND_TIMESTAMP0(
        "test_all", "TRACE_EVENT_COPY_BEGIN_WITH_ID_TID_AND_TIMESTAMP0 call",
        kAsyncId, kThreadId, TimeTicks::FromInternalValue(23456));

    TRACE_EVENT_BEGIN_WITH_ID_TID_AND_TIMESTAMP0(
        "test_all", "TRACE_EVENT_BEGIN_WITH_ID_TID_AND_TIMESTAMP0 call",
        kAsyncId2, kThreadId, TimeTicks::FromInternalValue(34567));
    TRACE_EVENT_ASYNC_STEP_PAST0("test_all",
                                 "TRACE_EVENT_ASYNC_STEP_PAST0 call", kAsyncId2,
                                 "step_end1");
    TRACE_EVENT_ASYNC_STEP_PAST1("test_all",
                                 "TRACE_EVENT_ASYNC_STEP_PAST1 call", kAsyncId2,
                                 "step_end2", "name1", "value1");

    TRACE_EVENT_END_WITH_ID_TID_AND_TIMESTAMP0(
        "test_all", "TRACE_EVENT_END_WITH_ID_TID_AND_TIMESTAMP0 call",
        kAsyncId2, kThreadId, TimeTicks::FromInternalValue(45678));

    TRACE_EVENT_OBJECT_CREATED_WITH_ID("test_all", "tracked object 1", 0x42);
    TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID("test_all", "tracked object 1", 0x42,
                                        "hello");
    TRACE_EVENT_OBJECT_DELETED_WITH_ID("test_all", "tracked object 1", 0x42);

    TraceScopedTrackableObject<int, kAllCategory> trackable("tracked object 2",
                                                            0x2128506);
    trackable.snapshot("world");

    TRACE_EVENT_OBJECT_CREATED_WITH_ID("test_all", "tracked object 3",
                                       TRACE_ID_WITH_SCOPE("scope", 0x42));
    TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID("test_all", "tracked object 3",
                                        TRACE_ID_WITH_SCOPE("scope", 0x42),
                                        "hello");
    TRACE_EVENT_OBJECT_DELETED_WITH_ID("test_all", "tracked object 3",
                                       TRACE_ID_WITH_SCOPE("scope", 0x42));

    TRACE_EVENT1(kControlCharacters, kControlCharacters,
                 kControlCharacters, kControlCharacters);

    uint64_t context_id = 0x20151021;

    TRACE_EVENT_ENTER_CONTEXT("test_all", "TRACE_EVENT_ENTER_CONTEXT call",
                              TRACE_ID_WITH_SCOPE("scope", context_id));
    TRACE_EVENT_LEAVE_CONTEXT("test_all", "TRACE_EVENT_LEAVE_CONTEXT call",
                              TRACE_ID_WITH_SCOPE("scope", context_id));

    TRACE_EVENT_ASYNC_BEGIN0("test_all", "async default process scope", 0x1000);
    TRACE_EVENT_ASYNC_BEGIN0("test_all", "async local id",
                             TRACE_ID_LOCAL(0x2000));
    TRACE_EVENT_ASYNC_BEGIN0("test_all", "async global id",
                             TRACE_ID_GLOBAL(0x3000));
    TRACE_EVENT_ASYNC_BEGIN0(
        "test_all", "async global id with scope string",
        TRACE_ID_WITH_SCOPE("scope string", TRACE_ID_GLOBAL(0x4000)));
  }  // Scope close causes TRACE_EVENT0 etc to send their END events.

  if (task_complete_event)
    task_complete_event->Signal();
}

void ValidateAllTraceMacrosCreatedData(const Value& trace_parsed) {
  const Value* item = nullptr;

#define EXPECT_FIND_(string) \
    item = FindTraceEntry(trace_parsed, string); \
    EXPECT_TRUE(item);
#define EXPECT_NOT_FIND_(string) \
    item = FindTraceEntry(trace_parsed, string); \
    EXPECT_FALSE(item);
#define EXPECT_SUB_FIND_(string) \
    if (item) \
      EXPECT_TRUE(IsStringInDict(string, item));

  EXPECT_FIND_("TRACE_EVENT0 call");
  {
    EXPECT_TRUE((item = FindTraceEntry(trace_parsed, "TRACE_EVENT0 call")));
    EXPECT_EQ(*item->FindStringKey("ph"), "X");
    item = FindTraceEntry(trace_parsed, "TRACE_EVENT0 call", item);
    EXPECT_FALSE(item);
  }
  EXPECT_FIND_("TRACE_EVENT1 call");
  EXPECT_SUB_FIND_("name1");
  EXPECT_SUB_FIND_("value1");
  EXPECT_FIND_("TRACE_EVENT2 call");
  EXPECT_SUB_FIND_("name1");
  EXPECT_SUB_FIND_("\"value1\"");
  EXPECT_SUB_FIND_("name2");
  EXPECT_SUB_FIND_("value\\2");

  EXPECT_FIND_("TRACE_EVENT_INSTANT0 call");
  { EXPECT_EQ(*item->FindStringKey("s"), "g"); }
  EXPECT_FIND_("TRACE_EVENT_INSTANT1 call");
  { EXPECT_EQ(*item->FindStringKey("s"), "p"); }
  EXPECT_SUB_FIND_("name1");
  EXPECT_SUB_FIND_("value1");
  EXPECT_FIND_("TRACE_EVENT_INSTANT2 call");
  { EXPECT_EQ(*item->FindStringKey("s"), "t"); }
  EXPECT_SUB_FIND_("name1");
  EXPECT_SUB_FIND_("value1");
  EXPECT_SUB_FIND_("name2");
  EXPECT_SUB_FIND_("value2");

  EXPECT_FIND_("TRACE_EVENT_BEGIN0 call");
  EXPECT_FIND_("TRACE_EVENT_BEGIN1 call");
  EXPECT_SUB_FIND_("name1");
  EXPECT_SUB_FIND_("value1");
  EXPECT_FIND_("TRACE_EVENT_BEGIN2 call");
  EXPECT_SUB_FIND_("name1");
  EXPECT_SUB_FIND_("value1");
  EXPECT_SUB_FIND_("name2");
  EXPECT_SUB_FIND_("value2");

  EXPECT_FIND_("TRACE_EVENT_ASYNC_BEGIN0 call");
  EXPECT_SUB_FIND_("id");
  EXPECT_SUB_FIND_(kAsyncIdStr);
  EXPECT_FIND_("TRACE_EVENT_ASYNC_BEGIN1 call");
  EXPECT_SUB_FIND_("id");
  EXPECT_SUB_FIND_(kAsyncIdStr);
  EXPECT_SUB_FIND_("name1");
  EXPECT_SUB_FIND_("value1");
  EXPECT_FIND_("TRACE_EVENT_ASYNC_BEGIN2 call");
  EXPECT_SUB_FIND_("id");
  EXPECT_SUB_FIND_(kAsyncIdStr);
  EXPECT_SUB_FIND_("name1");
  EXPECT_SUB_FIND_("value1");
  EXPECT_SUB_FIND_("name2");
  EXPECT_SUB_FIND_("value2");

  EXPECT_FIND_("TRACE_EVENT_ASYNC_STEP_INTO0 call");
  EXPECT_SUB_FIND_("id");
  EXPECT_SUB_FIND_(kAsyncIdStr);
  EXPECT_SUB_FIND_("step_begin1");
  EXPECT_FIND_("TRACE_EVENT_ASYNC_STEP_INTO1 call");
  EXPECT_SUB_FIND_("id");
  EXPECT_SUB_FIND_(kAsyncIdStr);
  EXPECT_SUB_FIND_("step_begin2");
  EXPECT_SUB_FIND_("name1");
  EXPECT_SUB_FIND_("value1");

  // Perfetto doesn't have separate begin and end events.
#if !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  EXPECT_FIND_("TRACE_EVENT_ASYNC_END0 call");
  EXPECT_SUB_FIND_("id");
  EXPECT_SUB_FIND_(kAsyncIdStr);
  EXPECT_FIND_("TRACE_EVENT_ASYNC_END1 call");
  EXPECT_SUB_FIND_("id");
  EXPECT_SUB_FIND_(kAsyncIdStr);
  EXPECT_SUB_FIND_("name1");
  EXPECT_SUB_FIND_("value1");
  EXPECT_FIND_("TRACE_EVENT_ASYNC_END2 call");
  EXPECT_SUB_FIND_("id");
  EXPECT_SUB_FIND_(kAsyncIdStr);
  EXPECT_SUB_FIND_("name1");
  EXPECT_SUB_FIND_("value1");
  EXPECT_SUB_FIND_("name2");
  EXPECT_SUB_FIND_("value2");
#endif  // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)

  EXPECT_FIND_("TRACE_COUNTER1 call");
  {
    EXPECT_EQ(*item->FindStringKey("ph"), "C");

    EXPECT_EQ(*item->FindIntPath("args.value"), 31415);
  }

  EXPECT_FIND_("TRACE_COUNTER2 call");
  {
    EXPECT_EQ(*item->FindStringKey("ph"), "C");

    EXPECT_EQ(*item->FindIntPath("args.a"), 30000);

    EXPECT_EQ(*item->FindIntPath("args.b"), 1415);
  }

  EXPECT_FIND_("TRACE_COUNTER_WITH_TIMESTAMP1 call");
  {
    EXPECT_EQ(*item->FindStringKey("ph"), "C");

    EXPECT_EQ(*item->FindIntPath("args.value"), 31415);

    EXPECT_EQ(*item->FindIntKey("ts"), 42);
  }

  EXPECT_FIND_("TRACE_COUNTER_WITH_TIMESTAMP2 call");
  {
    EXPECT_EQ(*item->FindStringKey("ph"), "C");

    EXPECT_EQ(*item->FindIntPath("args.a"), 30000);

    EXPECT_EQ(*item->FindIntPath("args.b"), 1415);

    EXPECT_EQ(*item->FindIntKey("ts"), 42);
  }

  EXPECT_FIND_("TRACE_COUNTER_ID1 call");
  {
    EXPECT_EQ(*item->FindStringKey("id"), "0x319009");

    EXPECT_EQ(*item->FindStringKey("ph"), "C");

    EXPECT_EQ(*item->FindIntPath("args.value"), 31415);
  }

  EXPECT_FIND_("TRACE_COUNTER_ID2 call");
  {
    EXPECT_EQ(*item->FindStringKey("id"), "0x319009");

    EXPECT_EQ(*item->FindStringKey("ph"), "C");

    EXPECT_EQ(*item->FindIntPath("args.a"), 30000);

    EXPECT_EQ(*item->FindIntPath("args.b"), 1415);
  }

  EXPECT_FIND_("TRACE_EVENT_COPY_BEGIN_WITH_ID_TID_AND_TIMESTAMP0 call");
  {
    EXPECT_EQ(*item->FindIntKey("ts"), 12345);

    EXPECT_EQ(*item->FindIntKey("tid"), kThreadId);

    EXPECT_EQ(*item->FindStringKey("id"), kAsyncIdStr);
  }

  EXPECT_FIND_("TRACE_EVENT_BEGIN_WITH_ID_TID_AND_TIMESTAMP0 call");
  {
    EXPECT_EQ(*item->FindIntKey("ts"), 34567);

    EXPECT_EQ(*item->FindIntKey("tid"), kThreadId);

    EXPECT_EQ(*item->FindStringKey("id"), kAsyncId2Str);
  }

  EXPECT_FIND_("TRACE_EVENT_ASYNC_STEP_PAST0 call");
  {
    EXPECT_SUB_FIND_("id");
    EXPECT_SUB_FIND_(kAsyncId2Str);
    EXPECT_SUB_FIND_("step_end1");
    EXPECT_FIND_("TRACE_EVENT_ASYNC_STEP_PAST1 call");
    EXPECT_SUB_FIND_("id");
    EXPECT_SUB_FIND_(kAsyncId2Str);
    EXPECT_SUB_FIND_("step_end2");
    EXPECT_SUB_FIND_("name1");
    EXPECT_SUB_FIND_("value1");
  }

  // Perfetto doesn't have separate begin and end events.
#if !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  EXPECT_FIND_("TRACE_EVENT_END_WITH_ID_TID_AND_TIMESTAMP0 call");
  {
    EXPECT_EQ(*item->FindIntKey("ts"), 45678);

    EXPECT_EQ(*item->FindIntKey("tid"), kThreadId);

    EXPECT_EQ(*item->FindStringKey("id"), kAsyncId2Str);
  }
#endif

  EXPECT_FIND_("tracked object 1");
  {
    EXPECT_EQ(*item->FindStringKey("ph"), "N");
    EXPECT_FALSE(item->FindKey("scope"));
    EXPECT_EQ(*item->FindStringKey("id"), "0x42");

    item = FindTraceEntry(trace_parsed, "tracked object 1", item);
    EXPECT_TRUE(item);
    EXPECT_EQ(*item->FindStringKey("ph"), "O");
    EXPECT_FALSE(item->FindKey("scope"));
    EXPECT_EQ(*item->FindStringKey("id"), "0x42");
    EXPECT_EQ(*item->FindStringPath("args.snapshot"), "hello");

    item = FindTraceEntry(trace_parsed, "tracked object 1", item);
    EXPECT_TRUE(item);
    EXPECT_EQ(*item->FindStringKey("ph"), "D");
    EXPECT_FALSE(item->FindKey("scope"));
    EXPECT_EQ(*item->FindStringKey("id"), "0x42");
  }

  EXPECT_FIND_("tracked object 2");
  {
    EXPECT_EQ(*item->FindStringKey("ph"), "N");
    EXPECT_EQ(*item->FindStringKey("id"), "0x2128506");

    item = FindTraceEntry(trace_parsed, "tracked object 2", item);
    EXPECT_TRUE(item);
    EXPECT_EQ(*item->FindStringKey("ph"), "O");
    EXPECT_EQ(*item->FindStringKey("id"), "0x2128506");
    EXPECT_EQ(*item->FindStringPath("args.snapshot"), "world");

    item = FindTraceEntry(trace_parsed, "tracked object 2", item);
    EXPECT_TRUE(item);
    EXPECT_EQ(*item->FindStringKey("ph"), "D");
    EXPECT_EQ(*item->FindStringKey("id"), "0x2128506");
  }

  EXPECT_FIND_("tracked object 3");
  {
    EXPECT_EQ(*item->FindStringKey("ph"), "N");
    EXPECT_EQ(*item->FindStringKey("scope"), "scope");
    EXPECT_EQ(*item->FindStringKey("id"), "0x42");

    item = FindTraceEntry(trace_parsed, "tracked object 3", item);
    EXPECT_TRUE(item);
    EXPECT_EQ(*item->FindStringKey("ph"), "O");
    EXPECT_EQ(*item->FindStringKey("scope"), "scope");
    EXPECT_EQ(*item->FindStringKey("id"), "0x42");
    EXPECT_EQ(*item->FindStringPath("args.snapshot"), "hello");

    item = FindTraceEntry(trace_parsed, "tracked object 3", item);
    EXPECT_TRUE(item);
    EXPECT_EQ(*item->FindStringKey("ph"), "D");
    EXPECT_EQ(*item->FindStringKey("scope"), "scope");
    EXPECT_EQ(*item->FindStringKey("id"), "0x42");
  }

  EXPECT_FIND_(kControlCharacters);
  EXPECT_SUB_FIND_(kControlCharacters);

  EXPECT_FIND_("TRACE_EVENT_ENTER_CONTEXT call");
  {
    EXPECT_EQ(*item->FindStringKey("ph"), "(");
    EXPECT_EQ(*item->FindStringKey("scope"), "scope");
    EXPECT_EQ(*item->FindStringKey("id"), "0x20151021");
  }

  EXPECT_FIND_("TRACE_EVENT_LEAVE_CONTEXT call");
  {
    EXPECT_EQ(*item->FindStringKey("ph"), ")");

    EXPECT_EQ(*item->FindStringKey("scope"), "scope");
    EXPECT_EQ(*item->FindStringKey("id"), "0x20151021");
  }

  EXPECT_FIND_("async default process scope");
  {
    EXPECT_EQ(*item->FindStringKey("ph"), "S");

    EXPECT_EQ(*item->FindStringKey("id"), "0x1000");
  }

  EXPECT_FIND_("async local id");
  {
    EXPECT_EQ(*item->FindStringKey("ph"), "S");

    EXPECT_EQ(*item->FindStringPath("id2.local"), "0x2000");
  }

  EXPECT_FIND_("async global id");
  {
    EXPECT_EQ(*item->FindStringKey("ph"), "S");

#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
    const char kIdPath[] = "id";
#else
    const char kIdPath[] = "id2.global";
#endif
    EXPECT_EQ(*item->FindStringPath(kIdPath), "0x3000");
  }

  EXPECT_FIND_("async global id with scope string");
  {
    EXPECT_EQ(*item->FindStringKey("ph"), "S");

#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
    const char kIdPath[] = "id";
#else
    const char kIdPath[] = "id2.global";
#endif
    EXPECT_EQ(*item->FindStringPath(kIdPath), "0x4000");

#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
    const char kExpectedScope[] = "test_all:scope string";
#else
    const char kExpectedScope[] = "scope string";
#endif

    EXPECT_EQ(*item->FindStringPath("scope"), kExpectedScope);
  }
}

void TraceManyInstantEvents(int thread_id, int num_events,
                            WaitableEvent* task_complete_event) {
  for (int i = 0; i < num_events; i++) {
    TRACE_EVENT_INSTANT2("test_all", "multi thread event",
                         TRACE_EVENT_SCOPE_THREAD, "thread", thread_id, "event",
                         i);
  }

  if (task_complete_event)
    task_complete_event->Signal();
}

void ValidateInstantEventPresentOnEveryThread(const Value& trace_parsed,
                                              int num_threads,
                                              int num_events) {
  std::map<int, std::map<int, bool>> results;

  for (const Value& value : trace_parsed.GetList()) {
    if (!value.is_dict())
      continue;

    const std::string* name = value.FindStringKey("name");
    if (!name || *name != "multi thread event")
      continue;

    absl::optional<int> maybe_thread = value.FindIntPath("args.thread");
    absl::optional<int> maybe_event = value.FindIntPath("args.event");

    EXPECT_TRUE(maybe_thread.has_value());
    EXPECT_TRUE(maybe_event.has_value());
    results[maybe_thread.value_or(0)][maybe_event.value_or(0)] = true;
  }

  EXPECT_FALSE(results[-1][-1]);
  for (int thread = 0; thread < num_threads; thread++) {
    for (int event = 0; event < num_events; event++) {
      EXPECT_TRUE(results[thread][event]);
    }
  }
}

void CheckTraceDefaultCategoryFilters(const TraceLog& trace_log) {
  // Default enables all category filters except the disabled-by-default-* ones.
  EXPECT_TRUE(IsCategoryEnabled("foo"));
  EXPECT_TRUE(IsCategoryEnabled("bar"));
  EXPECT_TRUE(IsCategoryEnabled("foo,bar"));
  EXPECT_TRUE(IsCategoryEnabled("foo,disabled-by-default-foo"));
  EXPECT_FALSE(
      IsCategoryEnabled("disabled-by-default-foo,disabled-by-default-bar"));
}

}  // namespace

// Simple Test for emitting data and validating it was received.
TEST_F(TraceEventTestFixture, DataCaptured) {
  TraceLog::GetInstance()->SetEnabled(TraceConfig(kRecordAllCategoryFilter, ""),
                                      TraceLog::RECORDING_MODE);

  TraceWithAllMacroVariants(nullptr);

  EndTraceAndFlush();

  ValidateAllTraceMacrosCreatedData(trace_parsed_);
}

// Emit some events and validate that only empty strings are received
// if we tell Flush() to discard events.
TEST_F(TraceEventTestFixture, DataDiscarded) {
  TraceLog::GetInstance()->SetEnabled(TraceConfig(kRecordAllCategoryFilter, ""),
                                      TraceLog::RECORDING_MODE);

  TraceWithAllMacroVariants(nullptr);

  CancelTrace();

  EXPECT_TRUE(trace_parsed_.GetList().empty());
}

class MockEnabledStateChangedObserver :
      public TraceLog::EnabledStateObserver {
 public:
  MOCK_METHOD0(OnTraceLogEnabled, void());
  MOCK_METHOD0(OnTraceLogDisabled, void());
};

TEST_F(TraceEventTestFixture, EnabledObserverFiresOnEnable) {
  MockEnabledStateChangedObserver observer;
  TraceLog::GetInstance()->AddEnabledStateObserver(&observer);

  EXPECT_CALL(observer, OnTraceLogEnabled())
      .Times(1);
  TraceLog::GetInstance()->SetEnabled(TraceConfig(kRecordAllCategoryFilter, ""),
                                      TraceLog::RECORDING_MODE);
  testing::Mock::VerifyAndClear(&observer);
  EXPECT_TRUE(TraceLog::GetInstance()->IsEnabled());

  // Cleanup.
  TraceLog::GetInstance()->RemoveEnabledStateObserver(&observer);
  TraceLog::GetInstance()->SetDisabled();
}

// Nested TraceLog enabling isn't supported by Perfetto.
#if !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
TEST_F(TraceEventTestFixture, EnabledObserverDoesntFireOnSecondEnable) {
  TraceLog::GetInstance()->SetEnabled(TraceConfig(kRecordAllCategoryFilter, ""),
                                      TraceLog::RECORDING_MODE);

  testing::StrictMock<MockEnabledStateChangedObserver> observer;
  TraceLog::GetInstance()->AddEnabledStateObserver(&observer);

  EXPECT_CALL(observer, OnTraceLogEnabled())
      .Times(0);
  EXPECT_CALL(observer, OnTraceLogDisabled())
      .Times(0);
  TraceLog::GetInstance()->SetEnabled(TraceConfig(kRecordAllCategoryFilter, ""),
                                      TraceLog::RECORDING_MODE);
  testing::Mock::VerifyAndClear(&observer);
  EXPECT_TRUE(TraceLog::GetInstance()->IsEnabled());

  // Cleanup.
  TraceLog::GetInstance()->RemoveEnabledStateObserver(&observer);
  TraceLog::GetInstance()->SetDisabled();
  TraceLog::GetInstance()->SetDisabled();
}

TEST_F(TraceEventTestFixture, EnabledObserverFiresOnFirstDisable) {
  TraceConfig tc_inc_all("*", "");
  TraceLog::GetInstance()->SetEnabled(tc_inc_all, TraceLog::RECORDING_MODE);
  TraceLog::GetInstance()->SetEnabled(tc_inc_all, TraceLog::RECORDING_MODE);

  testing::StrictMock<MockEnabledStateChangedObserver> observer;
  TraceLog::GetInstance()->AddEnabledStateObserver(&observer);

  EXPECT_CALL(observer, OnTraceLogEnabled())
      .Times(0);
  EXPECT_CALL(observer, OnTraceLogDisabled())
      .Times(1);
  TraceLog::GetInstance()->SetDisabled();
  testing::Mock::VerifyAndClear(&observer);

  // Cleanup.
  TraceLog::GetInstance()->RemoveEnabledStateObserver(&observer);
  TraceLog::GetInstance()->SetDisabled();
}
#endif  // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)

TEST_F(TraceEventTestFixture, EnabledObserverFiresOnDisable) {
  TraceLog::GetInstance()->SetEnabled(TraceConfig(kRecordAllCategoryFilter, ""),
                                      TraceLog::RECORDING_MODE);

  MockEnabledStateChangedObserver observer;
  TraceLog::GetInstance()->AddEnabledStateObserver(&observer);

  EXPECT_CALL(observer, OnTraceLogDisabled())
      .Times(1);
  TraceLog::GetInstance()->SetDisabled();
  testing::Mock::VerifyAndClear(&observer);

  // Cleanup.
  TraceLog::GetInstance()->RemoveEnabledStateObserver(&observer);
}

TEST_F(TraceEventTestFixture, EnabledObserverOwnedByTraceLog) {
  auto observer = std::make_unique<MockEnabledStateChangedObserver>();
  EXPECT_CALL(*observer, OnTraceLogEnabled()).Times(1);
  EXPECT_CALL(*observer, OnTraceLogDisabled()).Times(1);
  TraceLog::GetInstance()->AddOwnedEnabledStateObserver(std::move(observer));
  TraceLog::GetInstance()->SetEnabled(TraceConfig(kRecordAllCategoryFilter, ""),
                                      TraceLog::RECORDING_MODE);
  TraceLog::GetInstance()->SetDisabled();
  TraceLog::ResetForTesting();
  // These notifications won't be sent.
  TraceLog::GetInstance()->SetEnabled(TraceConfig(kRecordAllCategoryFilter, ""),
                                      TraceLog::RECORDING_MODE);
  TraceLog::GetInstance()->SetDisabled();
}

// Tests the IsEnabled() state of TraceLog changes before callbacks.
class AfterStateChangeEnabledStateObserver
    : public TraceLog::EnabledStateObserver {
 public:
  AfterStateChangeEnabledStateObserver() = default;
  ~AfterStateChangeEnabledStateObserver() override = default;

  // TraceLog::EnabledStateObserver overrides:
  void OnTraceLogEnabled() override {
    EXPECT_TRUE(TraceLog::GetInstance()->IsEnabled());
  }

  void OnTraceLogDisabled() override {
    // Perfetto intentionally notifies observers before tracing is disabled so
    // that final trace events may still be written.
#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
    EXPECT_TRUE(TraceLog::GetInstance()->IsEnabled());
#else
    EXPECT_FALSE(TraceLog::GetInstance()->IsEnabled());
#endif
  }
};

TEST_F(TraceEventTestFixture, ObserversFireAfterStateChange) {
  AfterStateChangeEnabledStateObserver observer;
  TraceLog::GetInstance()->AddEnabledStateObserver(&observer);

  TraceLog::GetInstance()->SetEnabled(TraceConfig(kRecordAllCategoryFilter, ""),
                                      TraceLog::RECORDING_MODE);
  EXPECT_TRUE(TraceLog::GetInstance()->IsEnabled());

  TraceLog::GetInstance()->SetDisabled();
  EXPECT_FALSE(TraceLog::GetInstance()->IsEnabled());

  TraceLog::GetInstance()->RemoveEnabledStateObserver(&observer);
}

// Tests that a state observer can remove itself during a callback.
class SelfRemovingEnabledStateObserver
    : public TraceLog::EnabledStateObserver {
 public:
  SelfRemovingEnabledStateObserver() = default;
  ~SelfRemovingEnabledStateObserver() override = default;

  // TraceLog::EnabledStateObserver overrides:
  void OnTraceLogEnabled() override {}

  void OnTraceLogDisabled() override {
    TraceLog::GetInstance()->RemoveEnabledStateObserver(this);
  }
};

// Self removing observers are not supported at the moment.
// TODO(alph): We could add support once we have recursive locks.
TEST_F(TraceEventTestFixture, DISABLED_SelfRemovingObserver) {
  ASSERT_EQ(0u, TraceLog::GetInstance()->GetObserverCountForTest());

  SelfRemovingEnabledStateObserver observer;
  TraceLog::GetInstance()->AddEnabledStateObserver(&observer);
  EXPECT_EQ(1u, TraceLog::GetInstance()->GetObserverCountForTest());

  TraceLog::GetInstance()->SetEnabled(TraceConfig(kRecordAllCategoryFilter, ""),
                                      TraceLog::RECORDING_MODE);
  TraceLog::GetInstance()->SetDisabled();
  // The observer removed itself on disable.
  EXPECT_EQ(0u, TraceLog::GetInstance()->GetObserverCountForTest());
}

bool IsNewTrace() {
  bool is_new_trace;
  TRACE_EVENT_IS_NEW_TRACE(&is_new_trace);
  return is_new_trace;
}

TEST_F(TraceEventTestFixture, NewTraceRecording) {
  ASSERT_FALSE(IsNewTrace());
  TraceLog::GetInstance()->SetEnabled(TraceConfig(kRecordAllCategoryFilter, ""),
                                      TraceLog::RECORDING_MODE);
  // First call to IsNewTrace() should succeed. But, the second shouldn't.
  ASSERT_TRUE(IsNewTrace());
  ASSERT_FALSE(IsNewTrace());
  EndTraceAndFlush();

  // IsNewTrace() should definitely be false now.
  ASSERT_FALSE(IsNewTrace());

  // Start another trace. IsNewTrace() should become true again, briefly, as
  // before.
  TraceLog::GetInstance()->SetEnabled(TraceConfig(kRecordAllCategoryFilter, ""),
                                      TraceLog::RECORDING_MODE);
  ASSERT_TRUE(IsNewTrace());
  ASSERT_FALSE(IsNewTrace());

  // Cleanup.
  EndTraceAndFlush();
}

// This test is not meaningful with Perfetto's deferred flushing behavior.
#if !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
TEST_F(TraceEventTestFixture, TestTraceFlush) {
  size_t min_traces = 1;
  size_t max_traces = 1;
  do {
    max_traces *= 2;
    TraceLog::GetInstance()->SetEnabled(TraceConfig(),
                                        TraceLog::RECORDING_MODE);
    for (size_t i = 0; i < max_traces; i++) {
      TRACE_EVENT_INSTANT0("test_x", "y", TRACE_EVENT_SCOPE_THREAD);
    }
    EndTraceAndFlush();
  } while (num_flush_callbacks_ < 2);

  while (min_traces + 50 <  max_traces) {
    size_t traces = (min_traces + max_traces) / 2;
    TraceLog::GetInstance()->SetEnabled(TraceConfig(),
                                        TraceLog::RECORDING_MODE);
    for (size_t i = 0; i < traces; i++) {
      TRACE_EVENT_INSTANT0("test_x", "y", TRACE_EVENT_SCOPE_THREAD);
    }
    EndTraceAndFlush();
    if (num_flush_callbacks_ < 2) {
      min_traces = traces - 10;
    } else {
      max_traces = traces + 10;
    }
  }

  for (size_t traces = min_traces; traces < max_traces; traces++) {
    TraceLog::GetInstance()->SetEnabled(TraceConfig(),
                                        TraceLog::RECORDING_MODE);
    for (size_t i = 0; i < traces; i++) {
      TRACE_EVENT_INSTANT0("test_x", "y", TRACE_EVENT_SCOPE_THREAD);
    }
    EndTraceAndFlush();
  }
}
#endif  // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)

TEST_F(TraceEventTestFixture, AddMetadataEvent) {
  int num_calls = 0;

  class Convertable : public ConvertableToTraceFormat {
   public:
    explicit Convertable(int* num_calls) : num_calls_(num_calls) {}
    ~Convertable() override = default;
    void AppendAsTraceFormat(std::string* out) const override {
      (*num_calls_)++;
      out->append("\"metadata_value\"");
    }

   private:
    int* num_calls_;
  };

  std::unique_ptr<ConvertableToTraceFormat> conv1(new Convertable(&num_calls));
  std::unique_ptr<Convertable> conv2(new Convertable(&num_calls));

  BeginTrace();
  TRACE_EVENT_API_ADD_METADATA_EVENT(
      TraceLog::GetCategoryGroupEnabled("__metadata"), "metadata_event_1",
      "metadata_arg_name", std::move(conv1));
  TRACE_EVENT_API_ADD_METADATA_EVENT(
      TraceLog::GetCategoryGroupEnabled("__metadata"), "metadata_event_2",
      "metadata_arg_name", std::move(conv2));
  // |AppendAsTraceFormat| should only be called on flush, not when the event
  // is added.
  ASSERT_EQ(0, num_calls);
  EndTraceAndFlush();
  ASSERT_EQ(2, num_calls);
  EXPECT_TRUE(FindNamePhaseKeyValue("metadata_event_1", "M",
                                    "metadata_arg_name", "metadata_value"));
  EXPECT_TRUE(FindNamePhaseKeyValue("metadata_event_2", "M",
                                    "metadata_arg_name", "metadata_value"));

  // The metadata event should only be adde to the current trace. In this new
  // trace, the event should not appear.
  BeginTrace();
  EndTraceAndFlush();
  ASSERT_EQ(2, num_calls);
}

// Test that categories work.
TEST_F(TraceEventTestFixture, Categories) {
  const std::vector<std::string> empty_categories;
  std::vector<std::string> included_categories;
  std::vector<std::string> excluded_categories;

  // Test that category filtering works.

  // Include nonexistent category -> no events
  Clear();
  included_categories.clear();
  TraceLog::GetInstance()->SetEnabled(TraceConfig("not_found823564786", ""),
                                      TraceLog::RECORDING_MODE);
  TRACE_EVENT_INSTANT0("cat1", "name", TRACE_EVENT_SCOPE_THREAD);
  TRACE_EVENT_INSTANT0("cat2", "name", TRACE_EVENT_SCOPE_THREAD);
  EndTraceAndFlush();
  DropTracedMetadataRecords();
  EXPECT_TRUE(trace_parsed_.GetList().empty());

  // Include existent category -> only events of that category
  Clear();
  included_categories.clear();
  TraceLog::GetInstance()->SetEnabled(TraceConfig("test_inc", ""),
                                      TraceLog::RECORDING_MODE);
  TRACE_EVENT_INSTANT0("test_inc", "name", TRACE_EVENT_SCOPE_THREAD);
  TRACE_EVENT_INSTANT0("test_inc2", "name", TRACE_EVENT_SCOPE_THREAD);
  EndTraceAndFlush();
  DropTracedMetadataRecords();
  EXPECT_TRUE(FindMatchingValue("cat", "test_inc"));
  EXPECT_FALSE(FindNonMatchingValue("cat", "test_inc"));

  // Include existent wildcard -> all categories matching wildcard
  Clear();
  included_categories.clear();
  TraceLog::GetInstance()->SetEnabled(TraceConfig("test_inc_wildcard_*", ""),
                                      TraceLog::RECORDING_MODE);
  TRACE_EVENT_INSTANT0("test_inc_wildcard_abc", "included",
                       TRACE_EVENT_SCOPE_THREAD);
  TRACE_EVENT_INSTANT0("test_inc_wildcard_", "included",
                       TRACE_EVENT_SCOPE_THREAD);
  TRACE_EVENT_INSTANT0("cat1", "not_inc", TRACE_EVENT_SCOPE_THREAD);
  TRACE_EVENT_INSTANT0("cat2", "not_inc", TRACE_EVENT_SCOPE_THREAD);
  TRACE_EVENT_INSTANT0("test_inc_wildcard_category,test_other_category",
                       "included", TRACE_EVENT_SCOPE_THREAD);
  TRACE_EVENT_INSTANT0("test_non_included_category,test_inc_wildcard_category",
                       "included", TRACE_EVENT_SCOPE_THREAD);
  EndTraceAndFlush();
  EXPECT_TRUE(FindMatchingValue("cat", "test_inc_wildcard_abc"));
  EXPECT_TRUE(FindMatchingValue("cat", "test_inc_wildcard_"));
  EXPECT_FALSE(FindMatchingValue("name", "not_inc"));
  EXPECT_TRUE(FindMatchingValue(
      "cat", "test_inc_wildcard_category,test_other_category"));
  EXPECT_TRUE(FindMatchingValue(
      "cat", "test_non_included_category,test_inc_wildcard_category"));

  included_categories.clear();

  // Exclude nonexistent category -> all events
  Clear();
  TraceLog::GetInstance()->SetEnabled(TraceConfig("-not_found823564786", ""),
                                      TraceLog::RECORDING_MODE);
  TRACE_EVENT_INSTANT0("cat1", "name", TRACE_EVENT_SCOPE_THREAD);
  TRACE_EVENT_INSTANT0("cat2", "name", TRACE_EVENT_SCOPE_THREAD);
  TRACE_EVENT_INSTANT0("category1,category2", "name", TRACE_EVENT_SCOPE_THREAD);
  EndTraceAndFlush();
  EXPECT_TRUE(FindMatchingValue("cat", "cat1"));
  EXPECT_TRUE(FindMatchingValue("cat", "cat2"));
  EXPECT_TRUE(FindMatchingValue("cat", "category1,category2"));

  // Exclude existent category -> only events of other categories
  Clear();
  TraceLog::GetInstance()->SetEnabled(TraceConfig("-test_inc", ""),
                                      TraceLog::RECORDING_MODE);
  TRACE_EVENT_INSTANT0("test_inc", "name", TRACE_EVENT_SCOPE_THREAD);
  TRACE_EVENT_INSTANT0("test_inc2", "name", TRACE_EVENT_SCOPE_THREAD);
  TRACE_EVENT_INSTANT0("test_inc2,test_inc", "name", TRACE_EVENT_SCOPE_THREAD);
  TRACE_EVENT_INSTANT0("test_inc,test_inc2", "name", TRACE_EVENT_SCOPE_THREAD);
  EndTraceAndFlush();
  EXPECT_TRUE(FindMatchingValue("cat", "test_inc2"));
  EXPECT_FALSE(FindMatchingValue("cat", "test_inc"));
  EXPECT_TRUE(FindMatchingValue("cat", "test_inc2,test_inc"));
  EXPECT_TRUE(FindMatchingValue("cat", "test_inc,test_inc2"));

  // Exclude existent wildcard -> all categories not matching wildcard
  Clear();
  TraceLog::GetInstance()->SetEnabled(TraceConfig("-test_inc_wildcard_*", ""),
                                      TraceLog::RECORDING_MODE);
  TRACE_EVENT_INSTANT0("test_inc_wildcard_abc", "not_inc",
                       TRACE_EVENT_SCOPE_THREAD);
  TRACE_EVENT_INSTANT0("test_inc_wildcard_", "not_inc",
                       TRACE_EVENT_SCOPE_THREAD);
  TRACE_EVENT_INSTANT0("cat1", "included", TRACE_EVENT_SCOPE_THREAD);
  TRACE_EVENT_INSTANT0("cat2", "included", TRACE_EVENT_SCOPE_THREAD);
  EndTraceAndFlush();
  EXPECT_TRUE(FindMatchingValue("cat", "cat1"));
  EXPECT_TRUE(FindMatchingValue("cat", "cat2"));
  EXPECT_FALSE(FindMatchingValue("name", "not_inc"));
}


// Test ASYNC_BEGIN/END events
TEST_F(TraceEventTestFixture, AsyncBeginEndEvents) {
  BeginTrace();

  uint64_t id = 0xfeedbeeffeedbeefull;
  TRACE_EVENT_ASYNC_BEGIN0("cat", "name1", id);
  TRACE_EVENT_ASYNC_STEP_INTO0("cat", "name1", id, "step1");
  TRACE_EVENT_ASYNC_END0("cat", "name1", id);
  TRACE_EVENT_BEGIN0("cat", "name2");
  TRACE_EVENT_ASYNC_BEGIN0("cat", "name3", 0);
  TRACE_EVENT_ASYNC_STEP_PAST0("cat", "name3", 0, "step2");

  EndTraceAndFlush();

  EXPECT_TRUE(FindNamePhase("name1", "S"));
  EXPECT_TRUE(FindNamePhase("name1", "T"));
  EXPECT_TRUE(FindNamePhase("name1", "F"));

  std::string id_str;
  StringAppendF(&id_str, "0x%" PRIx64, id);

  EXPECT_TRUE(FindNamePhaseKeyValue("name1", "S", "id", id_str.c_str()));
  EXPECT_TRUE(FindNamePhaseKeyValue("name1", "T", "id", id_str.c_str()));
  EXPECT_TRUE(FindNamePhaseKeyValue("name1", "F", "id", id_str.c_str()));
  EXPECT_TRUE(FindNamePhaseKeyValue("name3", "S", "id", "0x0"));
  EXPECT_TRUE(FindNamePhaseKeyValue("name3", "p", "id", "0x0"));

  // BEGIN events should not have id
  EXPECT_FALSE(FindNamePhaseKeyValue("name2", "B", "id", "0"));
}

// Test ASYNC_BEGIN/END events
TEST_F(TraceEventTestFixture, AsyncBeginEndPointerNotMangled) {
  void* ptr = this;

  TraceLog::GetInstance()->SetProcessID(100);
  BeginTrace();
  TRACE_EVENT_ASYNC_BEGIN0("cat", "name1", ptr);
  TRACE_EVENT_ASYNC_BEGIN0("cat", "name2", ptr);
  EndTraceAndFlush();

  TraceLog::GetInstance()->SetProcessID(200);
  BeginTrace();
  TRACE_EVENT_ASYNC_BEGIN0("cat", "name1", ptr);
  TRACE_EVENT_ASYNC_END0("cat", "name1", ptr);
  EndTraceAndFlush();

  const Value* async_begin = FindNamePhase("name1", "S");
  const Value* async_begin2 = FindNamePhase("name2", "S");
  const Value* async_end = FindNamePhase("name1", "F");
  EXPECT_TRUE(async_begin);
  EXPECT_TRUE(async_begin2);
  EXPECT_TRUE(async_end);

  std::string async_begin_id_str = *async_begin->FindStringPath("id2.local");
  std::string async_begin2_id_str = *async_begin2->FindStringPath("id2.local");
  std::string async_end_id_str = *async_end->FindStringPath("id2.local");

  // Since all ids are process-local and not mangled, they should be equal.
  EXPECT_STREQ(async_begin_id_str.c_str(), async_begin2_id_str.c_str());
  EXPECT_STREQ(async_begin_id_str.c_str(), async_end_id_str.c_str());
}

// Test that static strings are not copied.
TEST_F(TraceEventTestFixture, StaticStringVsString) {
  TraceLog* tracer = TraceLog::GetInstance();
  // Make sure old events are flushed:
  EXPECT_EQ(0u, tracer->GetStatus().event_count);
  const unsigned char* category_group_enabled =
      TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED("base");

  {
    BeginTrace();
    // Test that string arguments are copied.
    TraceEventHandle handle1 =
        trace_event_internal::AddTraceEvent(
            TRACE_EVENT_PHASE_INSTANT, category_group_enabled, "name1",
            trace_event_internal::kGlobalScope, trace_event_internal::kNoId,
            0, trace_event_internal::kNoId,
            "arg1", std::string("argval"), "arg2", std::string("argval"));
    // Test that static TRACE_STR_COPY string arguments are copied.
    TraceEventHandle handle2 =
        trace_event_internal::AddTraceEvent(
            TRACE_EVENT_PHASE_INSTANT, category_group_enabled, "name2",
            trace_event_internal::kGlobalScope, trace_event_internal::kNoId,
            0, trace_event_internal::kNoId,
            "arg1", TRACE_STR_COPY("argval"),
            "arg2", TRACE_STR_COPY("argval"));
#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
    (void)handle1;
    (void)handle2;
#else   // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
    EXPECT_GT(tracer->GetStatus().event_count, 1u);
    const TraceEvent* event1 = tracer->GetEventByHandle(handle1);
    const TraceEvent* event2 = tracer->GetEventByHandle(handle2);
    ASSERT_TRUE(event1);
    ASSERT_TRUE(event2);
    EXPECT_STREQ("name1", event1->name());
    EXPECT_STREQ("name2", event2->name());
    EXPECT_FALSE(event1->parameter_copy_storage().empty());
    EXPECT_FALSE(event2->parameter_copy_storage().empty());
    EXPECT_GT(event1->parameter_copy_storage().size(), 0u);
    EXPECT_GT(event2->parameter_copy_storage().size(), 0u);
#endif  // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
    EndTraceAndFlush();
  }

  {
    BeginTrace();
    // Test that static literal string arguments are not copied.
    TraceEventHandle handle1 =
        trace_event_internal::AddTraceEvent(
            TRACE_EVENT_PHASE_INSTANT, category_group_enabled, "name1",
            trace_event_internal::kGlobalScope, trace_event_internal::kNoId,
            0, trace_event_internal::kNoId,
            "arg1", "argval", "arg2", "argval");
    // Test that static TRACE_STR_COPY NULL string arguments are not copied.
    const char* str1 = nullptr;
    const char* str2 = nullptr;
    TraceEventHandle handle2 =
        trace_event_internal::AddTraceEvent(
            TRACE_EVENT_PHASE_INSTANT, category_group_enabled, "name2",
            trace_event_internal::kGlobalScope, trace_event_internal::kNoId,
            0, trace_event_internal::kNoId,
            "arg1", TRACE_STR_COPY(str1),
            "arg2", TRACE_STR_COPY(str2));
#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
    (void)handle1;
    (void)handle2;
#else   // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
    EXPECT_GT(tracer->GetStatus().event_count, 1u);
    const TraceEvent* event1 = tracer->GetEventByHandle(handle1);
    const TraceEvent* event2 = tracer->GetEventByHandle(handle2);
    ASSERT_TRUE(event1);
    ASSERT_TRUE(event2);
    EXPECT_STREQ("name1", event1->name());
    EXPECT_STREQ("name2", event2->name());
    EXPECT_TRUE(event1->parameter_copy_storage().empty());
    EXPECT_TRUE(event2->parameter_copy_storage().empty());
#endif  // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
    EndTraceAndFlush();
  }
}

// Test that data sent from other threads is gathered
TEST_F(TraceEventTestFixture, DataCapturedOnThread) {
  BeginTrace();

  Thread thread("1");
  WaitableEvent task_complete_event(WaitableEvent::ResetPolicy::AUTOMATIC,
                                    WaitableEvent::InitialState::NOT_SIGNALED);
  thread.Start();

  thread.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&TraceWithAllMacroVariants, &task_complete_event));
  task_complete_event.Wait();
  thread.Stop();

  EndTraceAndFlush();
  ValidateAllTraceMacrosCreatedData(trace_parsed_);
}

// Test that data sent from multiple threads is gathered
TEST_F(TraceEventTestFixture, DataCapturedManyThreads) {
  BeginTrace();

  const int num_threads = 4;
  const int num_events = 4000;
  Thread* threads[num_threads];
  WaitableEvent* task_complete_events[num_threads];
  for (int i = 0; i < num_threads; i++) {
    threads[i] = new Thread(StringPrintf("Thread %d", i));
    task_complete_events[i] =
        new WaitableEvent(WaitableEvent::ResetPolicy::AUTOMATIC,
                          WaitableEvent::InitialState::NOT_SIGNALED);
    threads[i]->Start();
    threads[i]->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&TraceManyInstantEvents, i, num_events,
                                  task_complete_events[i]));
  }

  for (int i = 0; i < num_threads; i++) {
    task_complete_events[i]->Wait();
  }

  // Let half of the threads end before flush.
  for (int i = 0; i < num_threads / 2; i++) {
    threads[i]->Stop();
    delete threads[i];
    delete task_complete_events[i];
  }

  EndTraceAndFlushInThreadWithMessageLoop();
  ValidateInstantEventPresentOnEveryThread(trace_parsed_,
                                           num_threads, num_events);

  // Let the other half of the threads end after flush.
  for (int i = num_threads / 2; i < num_threads; i++) {
    threads[i]->Stop();
    delete threads[i];
    delete task_complete_events[i];
  }
}

// Test that thread and process names show up in the trace
TEST_F(TraceEventTestFixture, ThreadNames) {
  // Create threads before we enable tracing to make sure
  // that tracelog still captures them.
  const int kNumThreads = 4;
  const int kNumEvents = 10;
  Thread* threads[kNumThreads];
  PlatformThreadId thread_ids[kNumThreads];
  for (int i = 0; i < kNumThreads; i++)
    threads[i] = new Thread(StringPrintf("Thread %d", i));

  // Enable tracing.
  BeginTrace();

  // Now run some trace code on these threads.
  WaitableEvent* task_complete_events[kNumThreads];
  for (int i = 0; i < kNumThreads; i++) {
    task_complete_events[i] =
        new WaitableEvent(WaitableEvent::ResetPolicy::AUTOMATIC,
                          WaitableEvent::InitialState::NOT_SIGNALED);
    threads[i]->Start();
    thread_ids[i] = threads[i]->GetThreadId();
    threads[i]->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&TraceManyInstantEvents, i, kNumEvents,
                                  task_complete_events[i]));
  }
  for (int i = 0; i < kNumThreads; i++) {
    task_complete_events[i]->Wait();
  }

  // Shut things down.
  for (int i = 0; i < kNumThreads; i++) {
    threads[i]->Stop();
    delete threads[i];
    delete task_complete_events[i];
  }

  EndTraceAndFlush();

  // Make sure we get thread name metadata.
  // Note, the test suite may have created a ton of threads.
  // So, we'll have thread names for threads we didn't create.
  std::vector<const Value*> items =
      FindTraceEntries(trace_parsed_, "thread_name");
  for (const Value* item : items) {
    ASSERT_TRUE(item);
    ASSERT_TRUE(item->is_dict());

    absl::optional<int> maybe_tid = item->FindIntKey("tid");
    EXPECT_TRUE(maybe_tid.has_value());

    // See if this thread name is one of the threads we just created
    for (int j = 0; j < kNumThreads; j++) {
      if (static_cast<int>(thread_ids[j]) != maybe_tid.value())
        continue;

      EXPECT_EQ(*item->FindStringKey("ph"), "M");
      EXPECT_EQ(*item->FindIntKey("pid"),
                static_cast<int>(base::GetCurrentProcId()));

      // If the thread name changes or the tid gets reused, the name will be
      // a comma-separated list of thread names, so look for a substring.
      std::string expected_name = StringPrintf("Thread %d", j);
      const std::string* name = item->FindStringPath("args.name");
      EXPECT_TRUE(name && name->find(expected_name) != std::string::npos);
    }
  }
}

// Test that the disabled trace categories are included/excluded from the
// trace output correctly.
TEST_F(TraceEventTestFixture, DisabledCategories) {
  BeginTrace();
  TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("cc"), "first",
                       TRACE_EVENT_SCOPE_THREAD);
  TRACE_EVENT_INSTANT0("test_included", "first", TRACE_EVENT_SCOPE_THREAD);
  EndTraceAndFlush();
  {
    const Value* item = nullptr;
    Value& trace_parsed = trace_parsed_;
    EXPECT_NOT_FIND_("disabled-by-default-cc");
    EXPECT_FIND_("test_included");
  }
  Clear();

  BeginSpecificTrace("disabled-by-default-cc");
  TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("cc"), "second",
                       TRACE_EVENT_SCOPE_THREAD);
  TRACE_EVENT_INSTANT0("test_other_included", "second",
                       TRACE_EVENT_SCOPE_THREAD);
  EndTraceAndFlush();

  {
    const Value* item = nullptr;
    Value& trace_parsed = trace_parsed_;
    EXPECT_FIND_("disabled-by-default-cc");
    EXPECT_FIND_("test_other_included");
  }

  Clear();

  BeginSpecificTrace("test_other_included");
  TRACE_EVENT_INSTANT0("test_other_included," TRACE_DISABLED_BY_DEFAULT("cc"),
                       "test_first", TRACE_EVENT_SCOPE_THREAD);
  TRACE_EVENT_INSTANT0(
      "test," TRACE_DISABLED_BY_DEFAULT("cc") ",test_other_included", "second",
      TRACE_EVENT_SCOPE_THREAD);
  EndTraceAndFlush();

  {
    const Value* item = nullptr;
    Value& trace_parsed = trace_parsed_;
    EXPECT_FIND_("test,disabled-by-default-cc,test_other_included");
    EXPECT_FIND_("test_other_included,disabled-by-default-cc");
  }
}

// Perfetto always traces all macro arguments immediately.
#if !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
TEST_F(TraceEventTestFixture, NormallyNoDeepCopy) {
  // Test that the TRACE_EVENT macros do not deep-copy their string. If they
  // do so it may indicate a performance regression, but more-over it would
  // make the DEEP_COPY overloads redundant.
  std::string name_string("event name");

  BeginTrace();
  TRACE_EVENT_INSTANT0("category", name_string.c_str(),
                       TRACE_EVENT_SCOPE_THREAD);

  // Modify the string in place (a wholesale reassignment may leave the old
  // string intact on the heap).
  name_string[0] = '@';

  EndTraceAndFlush();

  EXPECT_FALSE(FindTraceEntry(trace_parsed_, "event name"));
  EXPECT_TRUE(FindTraceEntry(trace_parsed_, name_string.c_str()));
}
#endif  // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)

TEST_F(TraceEventTestFixture, DeepCopy) {
  static const char kOriginalName1[] = "name1";
  static const char kOriginalName2[] = "name2";
  static const char kOriginalName3[] = "name3";
  std::string name1(kOriginalName1);
  std::string name2(kOriginalName2);
  std::string name3(kOriginalName3);
  std::string arg1("arg1");
  std::string arg2("arg2");
  std::string val1("val1");
  std::string val2("val2");

  BeginTrace();
  TRACE_EVENT_INSTANT_WITH_FLAGS0(
      "category", name1.c_str(),
      TRACE_EVENT_FLAG_COPY | TRACE_EVENT_SCOPE_THREAD);
  TRACE_EVENT_BEGIN_WITH_FLAGS1("category", name2.c_str(),
                                TRACE_EVENT_FLAG_COPY, arg1.c_str(), 5);
  TRACE_EVENT_END_WITH_FLAGS0("category", name2.c_str(), TRACE_EVENT_FLAG_COPY);
  TRACE_EVENT_COPY_BEGIN2("category", name3.c_str(), arg1.c_str(), val1,
                          arg2.c_str(), val2);

  // As per NormallyNoDeepCopy, modify the strings in place.
  name1[0] = name2[0] = name3[0] = arg1[0] = arg2[0] = val1[0] = val2[0] = '@';

  EndTraceAndFlush();

  EXPECT_FALSE(FindTraceEntry(trace_parsed_, name1.c_str()));
  EXPECT_FALSE(FindTraceEntry(trace_parsed_, name2.c_str()));
  EXPECT_FALSE(FindTraceEntry(trace_parsed_, name3.c_str()));

  const Value* entry1 = FindTraceEntry(trace_parsed_, kOriginalName1);
  const Value* entry2 = FindTraceEntry(trace_parsed_, kOriginalName2);
  const Value* entry3 = FindTraceEntry(trace_parsed_, kOriginalName3);
  ASSERT_TRUE(entry1);
  ASSERT_TRUE(entry2);
  ASSERT_TRUE(entry3);

  EXPECT_FALSE(entry2->FindIntPath("args.@rg1"));
  EXPECT_EQ(*entry2->FindIntPath("args.arg1"), 5);

  EXPECT_EQ(*entry3->FindStringPath("args.arg1"), "val1");
  EXPECT_EQ(*entry3->FindStringPath("args.arg2"), "val2");
}

// Test that TraceResultBuffer outputs the correct result whether it is added
// in chunks or added all at once.
TEST_F(TraceEventTestFixture, TraceResultBuffer) {
  Clear();

  trace_buffer_.Start();
  trace_buffer_.AddFragment("bla1");
  trace_buffer_.AddFragment("bla2");
  trace_buffer_.AddFragment("bla3,bla4");
  trace_buffer_.Finish();
  EXPECT_STREQ(json_output_.json_output.c_str(), "[bla1,bla2,bla3,bla4]");

  Clear();

  trace_buffer_.Start();
  trace_buffer_.AddFragment("bla1,bla2,bla3,bla4");
  trace_buffer_.Finish();
  EXPECT_STREQ(json_output_.json_output.c_str(), "[bla1,bla2,bla3,bla4]");
}

// Test that trace_event parameters are not evaluated if the tracing
// system is disabled.
TEST_F(TraceEventTestFixture, TracingIsLazy) {
  BeginTrace();

  int a = 0;
  TRACE_EVENT_INSTANT1("category", "test", TRACE_EVENT_SCOPE_THREAD, "a", a++);
  EXPECT_EQ(1, a);

  TraceLog::GetInstance()->SetDisabled();

  TRACE_EVENT_INSTANT1("category", "test", TRACE_EVENT_SCOPE_THREAD, "a", a++);
  EXPECT_EQ(1, a);

  EndTraceAndFlush();
}

TEST_F(TraceEventTestFixture, TraceEnableDisable) {
  TraceLog* trace_log = TraceLog::GetInstance();
  TraceConfig tc_inc_all("*", "");
  trace_log->SetEnabled(tc_inc_all, TraceLog::RECORDING_MODE);
  EXPECT_TRUE(trace_log->IsEnabled());
  trace_log->SetDisabled();
  EXPECT_FALSE(trace_log->IsEnabled());

  trace_log->SetEnabled(tc_inc_all, TraceLog::RECORDING_MODE);
  EXPECT_TRUE(trace_log->IsEnabled());
// Nested enable isn't supported by Perfetto.
#if !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  trace_log->SetEnabled(TraceConfig(), TraceLog::RECORDING_MODE);
  EXPECT_TRUE(trace_log->IsEnabled());
  trace_log->SetDisabled();
  EXPECT_FALSE(trace_log->IsEnabled());
#endif  // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  trace_log->SetDisabled();
  EXPECT_FALSE(trace_log->IsEnabled());
}

// Nested enable isn't supported by Perfetto.
#if !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
TEST_F(TraceEventTestFixture, TraceCategoriesAfterNestedEnable) {
  TraceLog* trace_log = TraceLog::GetInstance();
  trace_log->SetEnabled(TraceConfig("foo,bar", ""), TraceLog::RECORDING_MODE);
  EXPECT_TRUE(*trace_log->GetCategoryGroupEnabled("foo"));
  EXPECT_TRUE(*trace_log->GetCategoryGroupEnabled("bar"));
  EXPECT_FALSE(*trace_log->GetCategoryGroupEnabled("baz"));
  trace_log->SetEnabled(TraceConfig("foo2", ""), TraceLog::RECORDING_MODE);
  EXPECT_TRUE(*trace_log->GetCategoryGroupEnabled("foo2"));
  EXPECT_FALSE(*trace_log->GetCategoryGroupEnabled("baz"));
  // The "" becomes the default catergory set when applied.
  trace_log->SetEnabled(TraceConfig(), TraceLog::RECORDING_MODE);
  EXPECT_TRUE(*trace_log->GetCategoryGroupEnabled("foo"));
  EXPECT_TRUE(*trace_log->GetCategoryGroupEnabled("baz"));
  EXPECT_STREQ(
    "",
    trace_log->GetCurrentTraceConfig().ToCategoryFilterString().c_str());
  trace_log->SetDisabled();
  trace_log->SetDisabled();
  trace_log->SetDisabled();
  EXPECT_FALSE(*trace_log->GetCategoryGroupEnabled("foo"));
  EXPECT_FALSE(*trace_log->GetCategoryGroupEnabled("baz"));

  trace_log->SetEnabled(TraceConfig("-foo,-bar", ""), TraceLog::RECORDING_MODE);
  EXPECT_FALSE(*trace_log->GetCategoryGroupEnabled("foo"));
  EXPECT_TRUE(*trace_log->GetCategoryGroupEnabled("baz"));
  trace_log->SetEnabled(TraceConfig("moo", ""), TraceLog::RECORDING_MODE);
  EXPECT_TRUE(*trace_log->GetCategoryGroupEnabled("baz"));
  EXPECT_TRUE(*trace_log->GetCategoryGroupEnabled("moo"));
  EXPECT_FALSE(*trace_log->GetCategoryGroupEnabled("foo"));
  EXPECT_STREQ(
    "-foo,-bar",
    trace_log->GetCurrentTraceConfig().ToCategoryFilterString().c_str());
  trace_log->SetDisabled();
  trace_log->SetDisabled();

  // Make sure disabled categories aren't cleared if we set in the second.
  trace_log->SetEnabled(TraceConfig("disabled-by-default-cc,foo", ""),
                        TraceLog::RECORDING_MODE);
  EXPECT_FALSE(*trace_log->GetCategoryGroupEnabled("bar"));
  trace_log->SetEnabled(TraceConfig("disabled-by-default-gpu", ""),
                        TraceLog::RECORDING_MODE);
  EXPECT_TRUE(*trace_log->GetCategoryGroupEnabled("disabled-by-default-cc"));
  EXPECT_TRUE(*trace_log->GetCategoryGroupEnabled("disabled-by-default-gpu"));
  EXPECT_TRUE(*trace_log->GetCategoryGroupEnabled("bar"));
  EXPECT_STREQ(
    "disabled-by-default-cc,disabled-by-default-gpu",
    trace_log->GetCurrentTraceConfig().ToCategoryFilterString().c_str());
  trace_log->SetDisabled();
  trace_log->SetDisabled();
}
#endif  // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)

TEST_F(TraceEventTestFixture, TraceWithDefaultCategoryFilters) {
  TraceLog* trace_log = TraceLog::GetInstance();

  trace_log->SetEnabled(TraceConfig(), TraceLog::RECORDING_MODE);
  CheckTraceDefaultCategoryFilters(*trace_log);
  trace_log->SetDisabled();

  trace_log->SetEnabled(TraceConfig("", ""), TraceLog::RECORDING_MODE);
  CheckTraceDefaultCategoryFilters(*trace_log);
  trace_log->SetDisabled();

  trace_log->SetEnabled(TraceConfig("*", ""), TraceLog::RECORDING_MODE);
  CheckTraceDefaultCategoryFilters(*trace_log);
  trace_log->SetDisabled();

  trace_log->SetEnabled(TraceConfig(""), TraceLog::RECORDING_MODE);
  CheckTraceDefaultCategoryFilters(*trace_log);
  trace_log->SetDisabled();
}

// Flaky on iOS device, see crbug.com/908002
#if defined(OS_IOS) && !(TARGET_OS_SIMULATOR)
#define MAYBE_TraceWithDisabledByDefaultCategoryFilters DISABLED_TraceWithDisabledByDefaultCategoryFilters
#else
#define MAYBE_TraceWithDisabledByDefaultCategoryFilters TraceWithDisabledByDefaultCategoryFilters
#endif  // defined(OS_IOS) && !(TARGET_OS_SIMULATOR)
TEST_F(TraceEventTestFixture, MAYBE_TraceWithDisabledByDefaultCategoryFilters) {
  TraceLog* trace_log = TraceLog::GetInstance();

  trace_log->SetEnabled(TraceConfig("foo,disabled-by-default-foo", ""),
                        TraceLog::RECORDING_MODE);
  EXPECT_TRUE(IsCategoryEnabled("foo"));
  EXPECT_TRUE(IsCategoryEnabled("disabled-by-default-foo"));
  EXPECT_FALSE(IsCategoryEnabled("bar"));
  EXPECT_FALSE(IsCategoryEnabled("disabled-by-default-bar"));
  trace_log->SetDisabled();

  // Enabling only the disabled-by-default-* category means the default ones
  // are also enabled.
  trace_log->SetEnabled(TraceConfig("disabled-by-default-foo", ""),
                        TraceLog::RECORDING_MODE);
  EXPECT_TRUE(IsCategoryEnabled("disabled-by-default-foo"));
  EXPECT_TRUE(IsCategoryEnabled("foo"));
  EXPECT_TRUE(IsCategoryEnabled("bar"));
  EXPECT_FALSE(IsCategoryEnabled("disabled-by-default-bar"));
  trace_log->SetDisabled();
}

class MyData : public ConvertableToTraceFormat {
 public:
  MyData() = default;
  MyData(const MyData&) = delete;
  MyData& operator=(const MyData&) = delete;
  ~MyData() override = default;

  void AppendAsTraceFormat(std::string* out) const override {
    out->append("{\"foo\":1}");
  }
};

TEST_F(TraceEventTestFixture, ConvertableTypes) {
  TraceLog::GetInstance()->SetEnabled(TraceConfig(kRecordAllCategoryFilter, ""),
                                      TraceLog::RECORDING_MODE);

  {
    std::unique_ptr<ConvertableToTraceFormat> data(new MyData());
    std::unique_ptr<ConvertableToTraceFormat> data1(new MyData());
    std::unique_ptr<ConvertableToTraceFormat> data2(new MyData());
    TRACE_EVENT1("foo", "bar", "data", std::move(data));
    TRACE_EVENT2("foo", "baz", "data1", std::move(data1), "data2",
                 std::move(data2));
  }

  // Check that std::unique_ptr<DerivedClassOfConvertable> are properly treated
  // as convertable and not accidentally casted to bool.
  {
    std::unique_ptr<MyData> convertData1(new MyData());
    std::unique_ptr<MyData> convertData2(new MyData());
    std::unique_ptr<MyData> convertData3(new MyData());
    std::unique_ptr<MyData> convertData4(new MyData());
    TRACE_EVENT2("foo", "string_first", "str", "string value 1", "convert",
                 std::move(convertData1));
    TRACE_EVENT2("foo", "string_second", "convert", std::move(convertData2),
                 "str", "string value 2");
    TRACE_EVENT2("foo", "both_conv", "convert1", std::move(convertData3),
                 "convert2", std::move(convertData4));
  }
  EndTraceAndFlush();

  // One arg version.
  const Value* dict = FindNamePhase("bar", "X");
  ASSERT_TRUE(dict);

  const Value* args_dict = dict->FindDictKey("args");
  ASSERT_TRUE(args_dict);

  const Value* convertable_dict = args_dict->FindDictKey("data");
  ASSERT_TRUE(convertable_dict);

  EXPECT_EQ(*convertable_dict->FindIntKey("foo"), 1);

  // Two arg version.
  dict = FindNamePhase("baz", "X");
  ASSERT_TRUE(dict);

  args_dict = dict->FindDictKey("args");
  ASSERT_TRUE(args_dict);

  convertable_dict = args_dict->FindDictKey("data1");
  ASSERT_TRUE(convertable_dict);

  convertable_dict = args_dict->FindDictKey("data2");
  ASSERT_TRUE(convertable_dict);

  // Convertable with other types.
  dict = FindNamePhase("string_first", "X");
  ASSERT_TRUE(dict);

  args_dict = dict->FindDictKey("args");
  ASSERT_TRUE(args_dict);

  EXPECT_EQ(*args_dict->FindStringKey("str"), "string value 1");

  convertable_dict = args_dict->FindDictKey("convert");
  ASSERT_TRUE(convertable_dict);

  EXPECT_EQ(*convertable_dict->FindIntKey("foo"), 1);

  dict = FindNamePhase("string_second", "X");
  ASSERT_TRUE(dict);

  args_dict = dict->FindDictKey("args");
  ASSERT_TRUE(args_dict);

  EXPECT_EQ(*args_dict->FindStringKey("str"), "string value 2");

  convertable_dict = args_dict->FindDictKey("convert");
  ASSERT_TRUE(convertable_dict);

  EXPECT_EQ(*convertable_dict->FindIntKey("foo"), 1);

  dict = FindNamePhase("both_conv", "X");
  ASSERT_TRUE(dict);

  args_dict = dict->FindDictKey("args");
  ASSERT_TRUE(args_dict);

  convertable_dict = args_dict->FindDictKey("convert1");
  ASSERT_TRUE(convertable_dict);
  convertable_dict = args_dict->FindDictKey("convert2");
  ASSERT_TRUE(convertable_dict);
}

TEST_F(TraceEventTestFixture, PrimitiveArgs) {
  TraceLog::GetInstance()->SetEnabled(TraceConfig(kRecordAllCategoryFilter, ""),
                                      TraceLog::RECORDING_MODE);

  {
    TRACE_EVENT1("foo", "event1", "int_one", 1);
    TRACE_EVENT1("foo", "event2", "int_neg_ten", -10);
    TRACE_EVENT1("foo", "event3", "float_one", 1.0f);
    TRACE_EVENT1("foo", "event4", "float_half", .5f);
    TRACE_EVENT1("foo", "event5", "float_neghalf", -.5f);
    TRACE_EVENT1("foo", "event6", "float_infinity",
                 std::numeric_limits<float>::infinity());
    TRACE_EVENT1("foo", "event6b", "float_neg_infinity",
                 -std::numeric_limits<float>::infinity());
    TRACE_EVENT1("foo", "event7", "double_nan",
                 std::numeric_limits<double>::quiet_NaN());
    void* p = nullptr;
    TRACE_EVENT1("foo", "event8", "pointer_null", p);
    p = reinterpret_cast<void*>(0xbadf00d);
    TRACE_EVENT1("foo", "event9", "pointer_badf00d", p);
    TRACE_EVENT1("foo", "event10", "bool_true", true);
    TRACE_EVENT1("foo", "event11", "bool_false", false);
    TRACE_EVENT1("foo", "event12", "time_null", base::Time());
    TRACE_EVENT1("foo", "event13", "time_one",
                 base::Time::FromInternalValue(1));
    TRACE_EVENT1("foo", "event14", "timeticks_null", base::TimeTicks());
    TRACE_EVENT1("foo", "event15", "timeticks_one",
                 base::TimeTicks::FromInternalValue(1));
  }
  EndTraceAndFlush();

  const Value* args_dict = nullptr;
  const Value* dict = nullptr;
  std::string str_value;

  dict = FindNamePhase("event1", "X");
  ASSERT_TRUE(dict);
  args_dict = dict->FindDictKey("args");
  ASSERT_TRUE(args_dict);
  EXPECT_EQ(*args_dict->FindIntKey("int_one"), 1);

  dict = FindNamePhase("event2", "X");
  ASSERT_TRUE(dict);
  args_dict = dict->FindDictKey("args");
  ASSERT_TRUE(args_dict);
  EXPECT_EQ(*args_dict->FindIntKey("int_neg_ten"), -10);

  // 1f must be serlized to JSON as "1.0" in order to be a double, not an int.
  dict = FindNamePhase("event3", "X");
  ASSERT_TRUE(dict);
  args_dict = dict->FindDictKey("args");
  ASSERT_TRUE(args_dict);
  EXPECT_EQ(*args_dict->FindDoubleKey("float_one"), 1.0);

  // .5f must be serlized to JSON as "0.5".
  dict = FindNamePhase("event4", "X");
  ASSERT_TRUE(dict);
  args_dict = dict->FindDictKey("args");
  ASSERT_TRUE(args_dict);
  EXPECT_EQ(*args_dict->FindDoubleKey("float_half"), 0.5);

  // -.5f must be serlized to JSON as "-0.5".
  dict = FindNamePhase("event5", "X");
  ASSERT_TRUE(dict);
  args_dict = dict->FindDictKey("args");
  ASSERT_TRUE(args_dict);
  EXPECT_EQ(*args_dict->FindDoubleKey("float_neghalf"), -0.5);

  // Infinity is serialized to JSON as a string.
  dict = FindNamePhase("event6", "X");
  ASSERT_TRUE(dict);
  args_dict = dict->FindDictKey("args");
  ASSERT_TRUE(args_dict);
  EXPECT_EQ(*args_dict->FindStringKey("float_infinity"), "Infinity");
  dict = FindNamePhase("event6b", "X");
  ASSERT_TRUE(dict);
  args_dict = dict->FindDictKey("args");
  ASSERT_TRUE(args_dict);
  EXPECT_EQ(*args_dict->FindStringKey("float_neg_infinity"), "-Infinity");

  // NaN is serialized to JSON as a string.
  dict = FindNamePhase("event7", "X");
  ASSERT_TRUE(dict);
  args_dict = dict->FindDictKey("args");
  ASSERT_TRUE(args_dict);
  EXPECT_EQ(*args_dict->FindStringKey("double_nan"), "NaN");

  // NULL pointers should be serialized as "0x0".
  dict = FindNamePhase("event8", "X");
  ASSERT_TRUE(dict);
  args_dict = dict->FindDictKey("args");
  ASSERT_TRUE(args_dict);
  EXPECT_EQ(*args_dict->FindStringKey("pointer_null"), "0x0");

  // Other pointers should be serlized as a hex string.
  dict = FindNamePhase("event9", "X");
  ASSERT_TRUE(dict);
  args_dict = dict->FindDictKey("args");
  ASSERT_TRUE(args_dict);
  EXPECT_EQ(*args_dict->FindStringKey("pointer_badf00d"), "0xbadf00d");

  dict = FindNamePhase("event10", "X");
  ASSERT_TRUE(dict);
  args_dict = dict->FindDictKey("args");
  ASSERT_TRUE(args_dict);
  EXPECT_EQ(*args_dict->FindBoolKey("bool_true"), true);

  dict = FindNamePhase("event11", "X");
  ASSERT_TRUE(dict);
  args_dict = dict->FindDictKey("args");
  ASSERT_TRUE(args_dict);
  EXPECT_EQ(*args_dict->FindBoolKey("bool_false"), false);

  dict = FindNamePhase("event12", "X");
  ASSERT_TRUE(dict);
  args_dict = dict->FindDictKey("args");
  ASSERT_TRUE(args_dict);
  EXPECT_EQ(*args_dict->FindIntKey("time_null"), 0);

  dict = FindNamePhase("event13", "X");
  ASSERT_TRUE(dict);
  args_dict = dict->FindDictKey("args");
  ASSERT_TRUE(args_dict);
  EXPECT_EQ(*args_dict->FindIntKey("time_one"), 1);

  dict = FindNamePhase("event14", "X");
  ASSERT_TRUE(dict);
  args_dict = dict->FindDictKey("args");
  ASSERT_TRUE(args_dict);
  EXPECT_EQ(*args_dict->FindIntKey("timeticks_null"), 0);

  dict = FindNamePhase("event15", "X");
  ASSERT_TRUE(dict);
  args_dict = dict->FindDictKey("args");
  ASSERT_TRUE(args_dict);
  EXPECT_EQ(*args_dict->FindIntKey("timeticks_one"), 1);
}

TEST_F(TraceEventTestFixture, NameIsEscaped) {
  TraceLog::GetInstance()->SetEnabled(TraceConfig(kRecordAllCategoryFilter, ""),
                                      TraceLog::RECORDING_MODE);
  TRACE_EVENT0("category", "name\\with\\backspaces");
  EndTraceAndFlush();

  EXPECT_TRUE(FindMatchingValue("cat", "category"));
  EXPECT_TRUE(FindMatchingValue("name", "name\\with\\backspaces"));
}

// Runtime args filtering is not supported by Perfetto.
#if !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
namespace {

bool IsArgNameAllowed(const char* arg_name) {
  return base::MatchPattern(arg_name, "granular_arg_allowed");
}

bool IsTraceEventArgsAllowlisted(const char* category_group_name,
                                 const char* event_name,
                                 ArgumentNameFilterPredicate* arg_filter) {
  if (base::MatchPattern(category_group_name, "toplevel") &&
      base::MatchPattern(event_name, "*")) {
    return true;
  }

  if (base::MatchPattern(category_group_name, "benchmark") &&
      base::MatchPattern(event_name, "granularly_allowed")) {
    *arg_filter = base::BindRepeating(&IsArgNameAllowed);
    return true;
  }

  return false;
}

}  // namespace

TEST_F(TraceEventTestFixture, ArgsAllowlisting) {
  TraceLog::GetInstance()->SetArgumentFilterPredicate(
      base::BindRepeating(&IsTraceEventArgsAllowlisted));

  TraceLog::GetInstance()->SetEnabled(
    TraceConfig(kRecordAllCategoryFilter, "enable-argument-filter"),
    TraceLog::RECORDING_MODE);

  TRACE_EVENT1("toplevel", "event1", "int_one", 1);
  TRACE_EVENT1("Testing", "event2", "int_two", 1);

  TRACE_EVENT2("benchmark", "granularly_allowed", "granular_arg_allowed",
               "allowed_value", "granular_arg_disallowed", "disallowed_value");

  EndTraceAndFlush();

  const Value* args_dict = nullptr;
  const Value* dict = nullptr;

  dict = FindNamePhase("event1", "X");
  ASSERT_TRUE(dict);
  args_dict = dict->FindDictKey("args");
  ASSERT_TRUE(args_dict);
  EXPECT_EQ(*args_dict->FindIntKey("int_one"), 1);
  EXPECT_FALSE(args_dict->FindIntKey("int_two"));

  dict = FindNamePhase("event2", "X");
  ASSERT_TRUE(dict);
  EXPECT_EQ(*dict->FindStringKey("args"), "__stripped__");

  dict = FindNamePhase("granularly_allowed", "X");
  ASSERT_TRUE(dict);
  args_dict = dict->FindDictKey("args");
  ASSERT_TRUE(args_dict);

  EXPECT_EQ(*args_dict->FindStringKey("granular_arg_allowed"), "allowed_value");

  EXPECT_EQ(*args_dict->FindStringKey("granular_arg_disallowed"),
            "__stripped__");
}
#endif  // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)

// TraceBuffer isn't used with Perfetto.
#if !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
TEST_F(TraceEventTestFixture, TraceBufferVectorReportFull) {
  TraceLog* trace_log = TraceLog::GetInstance();
  trace_log->SetEnabled(
      TraceConfig(kRecordAllCategoryFilter, ""), TraceLog::RECORDING_MODE);
  trace_log->logged_events_.reset(
      TraceBuffer::CreateTraceBufferVectorOfSize(100));
  do {
    TRACE_EVENT_BEGIN_WITH_ID_TID_AND_TIMESTAMP0("test_all", "with_timestamp",
                                                 0, 0, TimeTicks::Now());
    TRACE_EVENT_END_WITH_ID_TID_AND_TIMESTAMP0("test_all", "with_timestamp", 0,
                                               0, TimeTicks::Now());
  } while (!trace_log->BufferIsFull());

  EndTraceAndFlush();

  const Value* trace_full_metadata = nullptr;

  trace_full_metadata = FindTraceEntry(trace_parsed_,
                                       "overflowed_at_ts");

  EXPECT_TRUE(trace_full_metadata);
  EXPECT_EQ(*trace_full_metadata->FindStringKey("ph"), "M");
  absl::optional<double> maybe_buffer_limit_reached_timestamp =
      trace_full_metadata->FindDoublePath("args.overflowed_at_ts");

  EXPECT_EQ(*maybe_buffer_limit_reached_timestamp,
            static_cast<double>(
                trace_log->buffer_limit_reached_timestamp_.ToInternalValue()));

  // Test that buffer_limit_reached_timestamp's value is between the timestamp
  // of the last trace event and current time.
  DropTracedMetadataRecords();
  ASSERT_TRUE(!trace_parsed_.GetList().empty());
  const Value& last_trace_event = trace_parsed_.GetList().back();
  EXPECT_TRUE(last_trace_event.is_dict());
  absl::optional<double> maybe_last_trace_event_timestamp =
      last_trace_event.FindDoubleKey("ts");
  EXPECT_TRUE(maybe_last_trace_event_timestamp.has_value());
  EXPECT_LE(maybe_last_trace_event_timestamp.value(),
            maybe_buffer_limit_reached_timestamp.value());
  EXPECT_LE(maybe_buffer_limit_reached_timestamp.value(),
            trace_log->OffsetNow().ToInternalValue());
}

TEST_F(TraceEventTestFixture, TraceBufferRingBufferGetReturnChunk) {
  TraceLog::GetInstance()->SetEnabled(
      TraceConfig(kRecordAllCategoryFilter, RECORD_CONTINUOUSLY),
      TraceLog::RECORDING_MODE);
  TraceBuffer* buffer = TraceLog::GetInstance()->trace_buffer();
  size_t capacity = buffer->Capacity();
  size_t num_chunks = capacity / TraceBufferChunk::kTraceBufferChunkSize;
  uint32_t last_seq = 0;
  size_t chunk_index;
  EXPECT_EQ(0u, buffer->Size());

  std::unique_ptr<TraceBufferChunk* []> chunks(
      new TraceBufferChunk*[num_chunks]);
  for (size_t i = 0; i < num_chunks; ++i) {
    chunks[i] = buffer->GetChunk(&chunk_index).release();
    EXPECT_TRUE(chunks[i]);
    EXPECT_EQ(i, chunk_index);
    EXPECT_GT(chunks[i]->seq(), last_seq);
    EXPECT_EQ((i + 1) * TraceBufferChunk::kTraceBufferChunkSize,
              buffer->Size());
    last_seq = chunks[i]->seq();
  }

  // Ring buffer is never full.
  EXPECT_FALSE(buffer->IsFull());

  // Return all chunks in original order.
  for (size_t i = 0; i < num_chunks; ++i)
    buffer->ReturnChunk(i, std::unique_ptr<TraceBufferChunk>(chunks[i]));

  // Should recycle the chunks in the returned order.
  for (size_t i = 0; i < num_chunks; ++i) {
    chunks[i] = buffer->GetChunk(&chunk_index).release();
    EXPECT_TRUE(chunks[i]);
    EXPECT_EQ(i, chunk_index);
    EXPECT_GT(chunks[i]->seq(), last_seq);
    last_seq = chunks[i]->seq();
  }

  // Return all chunks in reverse order.
  for (size_t i = 0; i < num_chunks; ++i) {
    buffer->ReturnChunk(num_chunks - i - 1, std::unique_ptr<TraceBufferChunk>(
                                                chunks[num_chunks - i - 1]));
  }

  // Should recycle the chunks in the returned order.
  for (size_t i = 0; i < num_chunks; ++i) {
    chunks[i] = buffer->GetChunk(&chunk_index).release();
    EXPECT_TRUE(chunks[i]);
    EXPECT_EQ(num_chunks - i - 1, chunk_index);
    EXPECT_GT(chunks[i]->seq(), last_seq);
    last_seq = chunks[i]->seq();
  }

  for (size_t i = 0; i < num_chunks; ++i)
    buffer->ReturnChunk(i, std::unique_ptr<TraceBufferChunk>(chunks[i]));

  TraceLog::GetInstance()->SetDisabled();
}

TEST_F(TraceEventTestFixture, TraceBufferRingBufferHalfIteration) {
  TraceLog::GetInstance()->SetEnabled(
      TraceConfig(kRecordAllCategoryFilter, RECORD_CONTINUOUSLY),
      TraceLog::RECORDING_MODE);
  TraceBuffer* buffer = TraceLog::GetInstance()->trace_buffer();
  size_t capacity = buffer->Capacity();
  size_t num_chunks = capacity / TraceBufferChunk::kTraceBufferChunkSize;
  size_t chunk_index;
  EXPECT_EQ(0u, buffer->Size());
  EXPECT_FALSE(buffer->NextChunk());

  size_t half_chunks = num_chunks / 2;
  std::unique_ptr<TraceBufferChunk* []> chunks(
      new TraceBufferChunk*[half_chunks]);

  for (size_t i = 0; i < half_chunks; ++i) {
    chunks[i] = buffer->GetChunk(&chunk_index).release();
    EXPECT_TRUE(chunks[i]);
    EXPECT_EQ(i, chunk_index);
  }
  for (size_t i = 0; i < half_chunks; ++i)
    buffer->ReturnChunk(i, std::unique_ptr<TraceBufferChunk>(chunks[i]));

  for (size_t i = 0; i < half_chunks; ++i)
    EXPECT_EQ(chunks[i], buffer->NextChunk());
  EXPECT_FALSE(buffer->NextChunk());
  TraceLog::GetInstance()->SetDisabled();
}

TEST_F(TraceEventTestFixture, TraceBufferRingBufferFullIteration) {
  TraceLog::GetInstance()->SetEnabled(
      TraceConfig(kRecordAllCategoryFilter, RECORD_CONTINUOUSLY),
      TraceLog::RECORDING_MODE);
  TraceBuffer* buffer = TraceLog::GetInstance()->trace_buffer();
  size_t capacity = buffer->Capacity();
  size_t num_chunks = capacity / TraceBufferChunk::kTraceBufferChunkSize;
  size_t chunk_index;
  EXPECT_EQ(0u, buffer->Size());
  EXPECT_FALSE(buffer->NextChunk());

  std::unique_ptr<TraceBufferChunk* []> chunks(
      new TraceBufferChunk*[num_chunks]);

  for (size_t i = 0; i < num_chunks; ++i) {
    chunks[i] = buffer->GetChunk(&chunk_index).release();
    EXPECT_TRUE(chunks[i]);
    EXPECT_EQ(i, chunk_index);
  }
  for (size_t i = 0; i < num_chunks; ++i)
    buffer->ReturnChunk(i, std::unique_ptr<TraceBufferChunk>(chunks[i]));

  for (size_t i = 0; i < num_chunks; ++i)
    EXPECT_TRUE(chunks[i] == buffer->NextChunk());
  EXPECT_FALSE(buffer->NextChunk());
  TraceLog::GetInstance()->SetDisabled();
}

TEST_F(TraceEventTestFixture, TraceRecordAsMuchAsPossibleMode) {
  TraceLog::GetInstance()->SetEnabled(
    TraceConfig(kRecordAllCategoryFilter, RECORD_AS_MUCH_AS_POSSIBLE),
    TraceLog::RECORDING_MODE);
  TraceBuffer* buffer = TraceLog::GetInstance()->trace_buffer();
  EXPECT_EQ(512000000UL, buffer->Capacity());
  TraceLog::GetInstance()->SetDisabled();
}

TEST_F(TraceEventTestFixture, ConfigTraceBufferLimit) {
  const size_t kLimit = 2048;
  TraceConfig config(kRecordAllCategoryFilter, RECORD_UNTIL_FULL);
  config.SetTraceBufferSizeInEvents(kLimit);
  TraceLog::GetInstance()->SetEnabled(config, TraceLog::RECORDING_MODE);
  TraceBuffer* buffer = TraceLog::GetInstance()->trace_buffer();
  EXPECT_EQ(kLimit, buffer->Capacity());
  TraceLog::GetInstance()->SetDisabled();
}
#endif  // BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)

void BlockUntilStopped(WaitableEvent* task_start_event,
                       WaitableEvent* task_stop_event) {
  task_start_event->Signal();
  task_stop_event->Wait();
}

TEST_F(TraceEventTestFixture, SetCurrentThreadBlocksMessageLoopBeforeTracing) {
  BeginTrace();

  Thread thread("1");
  WaitableEvent task_complete_event(WaitableEvent::ResetPolicy::AUTOMATIC,
                                    WaitableEvent::InitialState::NOT_SIGNALED);
  thread.Start();
  thread.task_runner()->PostTask(
      FROM_HERE, BindOnce(&TraceLog::SetCurrentThreadBlocksMessageLoop,
                          Unretained(TraceLog::GetInstance())));

  thread.task_runner()->PostTask(
      FROM_HERE, BindOnce(&TraceWithAllMacroVariants, &task_complete_event));
  task_complete_event.Wait();

  WaitableEvent task_start_event(WaitableEvent::ResetPolicy::AUTOMATIC,
                                 WaitableEvent::InitialState::NOT_SIGNALED);
  WaitableEvent task_stop_event(WaitableEvent::ResetPolicy::AUTOMATIC,
                                WaitableEvent::InitialState::NOT_SIGNALED);
  thread.task_runner()->PostTask(
      FROM_HERE,
      BindOnce(&BlockUntilStopped, &task_start_event, &task_stop_event));
  task_start_event.Wait();

  EndTraceAndFlush();
  ValidateAllTraceMacrosCreatedData(trace_parsed_);

  task_stop_event.Signal();
  thread.Stop();
}

TEST_F(TraceEventTestFixture, ConvertTraceConfigToInternalOptions) {
  TraceLog* trace_log = TraceLog::GetInstance();
  EXPECT_EQ(TraceLog::kInternalRecordUntilFull,
            trace_log->GetInternalOptionsFromTraceConfig(
                TraceConfig(kRecordAllCategoryFilter, RECORD_UNTIL_FULL)));

  EXPECT_EQ(TraceLog::kInternalRecordContinuously,
            trace_log->GetInternalOptionsFromTraceConfig(
                TraceConfig(kRecordAllCategoryFilter, RECORD_CONTINUOUSLY)));

  EXPECT_EQ(TraceLog::kInternalEchoToConsole,
            trace_log->GetInternalOptionsFromTraceConfig(
                TraceConfig(kRecordAllCategoryFilter, ECHO_TO_CONSOLE)));

  EXPECT_EQ(TraceLog::kInternalEchoToConsole,
            trace_log->GetInternalOptionsFromTraceConfig(
                TraceConfig("*", "trace-to-console,enable-systrace")));
}

void SetBlockingFlagAndBlockUntilStopped(WaitableEvent* task_start_event,
                                         WaitableEvent* task_stop_event) {
  TraceLog::GetInstance()->SetCurrentThreadBlocksMessageLoop();
  BlockUntilStopped(task_start_event, task_stop_event);
}

TEST_F(TraceEventTestFixture, SetCurrentThreadBlocksMessageLoopAfterTracing) {
  BeginTrace();

  Thread thread("1");
  WaitableEvent task_complete_event(WaitableEvent::ResetPolicy::AUTOMATIC,
                                    WaitableEvent::InitialState::NOT_SIGNALED);
  thread.Start();

  thread.task_runner()->PostTask(
      FROM_HERE, BindOnce(&TraceWithAllMacroVariants, &task_complete_event));
  task_complete_event.Wait();

  WaitableEvent task_start_event(WaitableEvent::ResetPolicy::AUTOMATIC,
                                 WaitableEvent::InitialState::NOT_SIGNALED);
  WaitableEvent task_stop_event(WaitableEvent::ResetPolicy::AUTOMATIC,
                                WaitableEvent::InitialState::NOT_SIGNALED);
  thread.task_runner()->PostTask(FROM_HERE,
                                 BindOnce(&SetBlockingFlagAndBlockUntilStopped,
                                          &task_start_event, &task_stop_event));
  task_start_event.Wait();

  EndTraceAndFlush();
  ValidateAllTraceMacrosCreatedData(trace_parsed_);

  task_stop_event.Signal();
  thread.Stop();
}

TEST_F(TraceEventTestFixture, ThreadOnceBlocking) {
  BeginTrace();

  Thread thread("1");
  WaitableEvent task_complete_event(WaitableEvent::ResetPolicy::AUTOMATIC,
                                    WaitableEvent::InitialState::NOT_SIGNALED);
  thread.Start();

  thread.task_runner()->PostTask(
      FROM_HERE, BindOnce(&TraceWithAllMacroVariants, &task_complete_event));
  task_complete_event.Wait();

  WaitableEvent task_start_event(WaitableEvent::ResetPolicy::AUTOMATIC,
                                 WaitableEvent::InitialState::NOT_SIGNALED);
  WaitableEvent task_stop_event(WaitableEvent::ResetPolicy::AUTOMATIC,
                                WaitableEvent::InitialState::NOT_SIGNALED);
  thread.task_runner()->PostTask(
      FROM_HERE,
      BindOnce(&BlockUntilStopped, &task_start_event, &task_stop_event));
  task_start_event.Wait();

  // The thread will timeout in this flush.
  EndTraceAndFlushInThreadWithMessageLoop();
  Clear();

  // Let the thread's message loop continue to spin.
  task_stop_event.Signal();

  // The following sequence ensures that the FlushCurrentThread task has been
  // executed in the thread before continuing.
  thread.task_runner()->PostTask(
      FROM_HERE,
      BindOnce(&BlockUntilStopped, &task_start_event, &task_stop_event));
  task_start_event.Wait();
  task_stop_event.Signal();
  Clear();

  // TraceLog should discover the generation mismatch and recover the thread
  // local buffer for the thread without any error.
  BeginTrace();
  thread.task_runner()->PostTask(
      FROM_HERE, BindOnce(&TraceWithAllMacroVariants, &task_complete_event));
  task_complete_event.Wait();
  EndTraceAndFlushInThreadWithMessageLoop();
  ValidateAllTraceMacrosCreatedData(trace_parsed_);
}

std::string* g_log_buffer = nullptr;
bool MockLogMessageHandler(int, const char*, int, size_t,
                           const std::string& str) {
  if (!g_log_buffer)
    g_log_buffer = new std::string();
  g_log_buffer->append(str);
  return false;
}

// Flaky on iOS device, see crbug.com/908002
#if defined(OS_IOS) && !(TARGET_OS_SIMULATOR)
#define MAYBE_EchoToConsole DISABLED_EchoToConsole
#else
#define MAYBE_EchoToConsole EchoToConsole
#endif  // defined(OS_IOS) && !(TARGET_OS_SIMULATOR)
TEST_F(TraceEventTestFixture, MAYBE_EchoToConsole) {
  logging::LogMessageHandlerFunction old_log_message_handler =
      logging::GetLogMessageHandler();
  logging::SetLogMessageHandler(MockLogMessageHandler);

  TraceLog::GetInstance()->SetEnabled(
      TraceConfig(kRecordAllCategoryFilter, ECHO_TO_CONSOLE),
      TraceLog::RECORDING_MODE);
  TRACE_EVENT_BEGIN0("test_a", "begin_end");
  {
    TRACE_EVENT0("test_b", "duration");
    TRACE_EVENT0("test_b1", "duration1");
  }
  TRACE_EVENT_INSTANT0("test_c", "instant", TRACE_EVENT_SCOPE_GLOBAL);
  TRACE_EVENT_END0("test_a", "begin_end");

  // Perfetto's console interceptor outputs directly to the console and has test
  // coverage upstream.
#if !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  EXPECT_NE(std::string::npos, g_log_buffer->find("begin_end[test_a]\x1b"));
  EXPECT_NE(std::string::npos, g_log_buffer->find("| duration[test_b]\x1b"));
  EXPECT_NE(std::string::npos,
            g_log_buffer->find("| | duration1[test_b1]\x1b"));
  EXPECT_NE(std::string::npos, g_log_buffer->find("| | duration1[test_b1] ("));
  EXPECT_NE(std::string::npos, g_log_buffer->find("| duration[test_b] ("));
  EXPECT_NE(std::string::npos, g_log_buffer->find("| instant[test_c]\x1b"));
  EXPECT_NE(std::string::npos, g_log_buffer->find("begin_end[test_a] ("));
#endif  // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)

  EndTraceAndFlush();
  delete g_log_buffer;
  logging::SetLogMessageHandler(old_log_message_handler);
  g_log_buffer = nullptr;
}

bool LogMessageHandlerWithTraceEvent(int, const char*, int, size_t,
                                     const std::string&) {
  TRACE_EVENT0("log", "trace_event");
  return false;
}

TEST_F(TraceEventTestFixture, EchoToConsoleTraceEventRecursion) {
  logging::LogMessageHandlerFunction old_log_message_handler =
      logging::GetLogMessageHandler();
  logging::SetLogMessageHandler(LogMessageHandlerWithTraceEvent);

  TraceLog::GetInstance()->SetEnabled(
      TraceConfig(kRecordAllCategoryFilter, ECHO_TO_CONSOLE),
      TraceLog::RECORDING_MODE);
  {
    // This should not cause deadlock or infinite recursion.
    TRACE_EVENT0("test_b", "duration");
  }

  EndTraceAndFlush();
  logging::SetLogMessageHandler(old_log_message_handler);
}

// Perfetto doesn't support overriding the time offset.
#if !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
TEST_F(TraceEventTestFixture, TimeOffset) {
  BeginTrace();
  // Let TraceLog timer start from 0.
  TimeDelta time_offset = TimeTicks::Now() - TimeTicks();
  TraceLog::GetInstance()->SetTimeOffset(time_offset);

  {
    TRACE_EVENT0("test_all", "duration1");
    TRACE_EVENT0("test_all", "duration2");
  }
  TRACE_EVENT_BEGIN_WITH_ID_TID_AND_TIMESTAMP0("test_all", "with_timestamp", 0,
                                               0, TimeTicks::Now());
  TRACE_EVENT_END_WITH_ID_TID_AND_TIMESTAMP0("test_all", "with_timestamp", 0, 0,
                                             TimeTicks::Now());

  EndTraceAndFlush();
  DropTracedMetadataRecords();

  double end_time = static_cast<double>(
      (TimeTicks::Now() - time_offset).ToInternalValue());
  double last_timestamp = 0;
  for (const Value& item : trace_parsed_.GetList()) {
    EXPECT_TRUE(item.is_dict());
    absl::optional<double> timestamp = item.FindDoubleKey("ts");
    EXPECT_TRUE(timestamp.has_value());
    EXPECT_GE(timestamp.value(), last_timestamp);
    EXPECT_LE(timestamp.value(), end_time);
    last_timestamp = timestamp.value();
  }
}
#endif  // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)

// Runtime filtering isn't supported with Perfetto.
#if !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
// Flaky on iOS device, see crbug.com/908002
#if defined(OS_IOS) && !(TARGET_OS_SIMULATOR)
#define MAYBE_TraceFilteringMode DISABLED_TraceFilteringMode
#else
#define MAYBE_TraceFilteringMode TraceFilteringMode
#endif  // defined(OS_IOS) && !(TARGET_OS_SIMULATOR)
TEST_F(TraceEventTestFixture, MAYBE_TraceFilteringMode) {
  const char config_json[] =
      "{"
      "  \"event_filters\": ["
      "     {"
      "       \"filter_predicate\": \"testing_predicate\", "
      "       \"included_categories\": [\"*\"]"
      "     }"
      "  ]"
      "}";

  // Run RECORDING_MODE within FILTERING_MODE:
  TestEventFilter::HitsCounter filter_hits_counter;
  TestEventFilter::set_filter_return_value(true);
  TraceLog::GetInstance()->SetFilterFactoryForTesting(TestEventFilter::Factory);

  // Only filtering mode is enabled with test filters.
  TraceLog::GetInstance()->SetEnabled(TraceConfig(config_json),
                                      TraceLog::FILTERING_MODE);
  EXPECT_EQ(TraceLog::FILTERING_MODE, TraceLog::GetInstance()->enabled_modes());
  {
    void* ptr = this;
    TRACE_EVENT0("test_c0", "name0");
    TRACE_EVENT_ASYNC_BEGIN0("test_c1", "name1", ptr);
    TRACE_EVENT_INSTANT0("test_c0", "name0", TRACE_EVENT_SCOPE_THREAD);
    TRACE_EVENT_ASYNC_END0("test_c1", "name1", ptr);
  }

  // Recording mode is enabled when filtering mode is turned on.
  TraceLog::GetInstance()->SetEnabled(TraceConfig("", ""),
                                      TraceLog::RECORDING_MODE);
  EXPECT_EQ(TraceLog::RECORDING_MODE | TraceLog::FILTERING_MODE,
            TraceLog::GetInstance()->enabled_modes());
  { TRACE_EVENT0("test_c2", "name2"); }
  // Only recording mode is disabled and filtering mode will continue to run.
  TraceLog::GetInstance()->SetDisabled(TraceLog::RECORDING_MODE);
  EXPECT_EQ(TraceLog::FILTERING_MODE, TraceLog::GetInstance()->enabled_modes());

  { TRACE_EVENT0("test_c0", "name0"); }
  // Filtering mode is disabled and no tracing mode should be enabled.
  TraceLog::GetInstance()->SetDisabled(TraceLog::FILTERING_MODE);
  EXPECT_EQ(0, TraceLog::GetInstance()->enabled_modes());

  EndTraceAndFlush();
  EXPECT_FALSE(FindMatchingValue("cat", "test_c0"));
  EXPECT_FALSE(FindMatchingValue("cat", "test_c1"));
  EXPECT_FALSE(FindMatchingValue("name", "name0"));
  EXPECT_FALSE(FindMatchingValue("name", "name1"));
  EXPECT_TRUE(FindMatchingValue("cat", "test_c2"));
  EXPECT_TRUE(FindMatchingValue("name", "name2"));
  EXPECT_EQ(6u, filter_hits_counter.filter_trace_event_hit_count);
  EXPECT_EQ(3u, filter_hits_counter.end_event_hit_count);
  Clear();
  filter_hits_counter.Reset();

  // Run FILTERING_MODE within RECORDING_MODE:
  // Only recording mode is enabled and all events must be recorded.
  TraceLog::GetInstance()->SetEnabled(TraceConfig("", ""),
                                      TraceLog::RECORDING_MODE);
  EXPECT_EQ(TraceLog::RECORDING_MODE, TraceLog::GetInstance()->enabled_modes());
  { TRACE_EVENT0("test_c0", "name0"); }

  // Filtering mode is also enabled and all events must be filtered-out.
  TestEventFilter::set_filter_return_value(false);
  TraceLog::GetInstance()->SetEnabled(TraceConfig(config_json),
                                      TraceLog::FILTERING_MODE);
  EXPECT_EQ(TraceLog::RECORDING_MODE | TraceLog::FILTERING_MODE,
            TraceLog::GetInstance()->enabled_modes());
  { TRACE_EVENT0("test_c1", "name1"); }
  // Only filtering mode is disabled and recording mode should continue to run
  // with all events being recorded.
  TraceLog::GetInstance()->SetDisabled(TraceLog::FILTERING_MODE);
  EXPECT_EQ(TraceLog::RECORDING_MODE, TraceLog::GetInstance()->enabled_modes());

  { TRACE_EVENT0("test_c2", "name2"); }
  // Recording mode is disabled and no tracing mode should be enabled.
  TraceLog::GetInstance()->SetDisabled(TraceLog::RECORDING_MODE);
  EXPECT_EQ(0, TraceLog::GetInstance()->enabled_modes());

  EndTraceAndFlush();
  EXPECT_TRUE(FindMatchingValue("cat", "test_c0"));
  EXPECT_TRUE(FindMatchingValue("cat", "test_c2"));
  EXPECT_TRUE(FindMatchingValue("name", "name0"));
  EXPECT_TRUE(FindMatchingValue("name", "name2"));
  EXPECT_FALSE(FindMatchingValue("cat", "test_c1"));
  EXPECT_FALSE(FindMatchingValue("name", "name1"));
  EXPECT_EQ(1u, filter_hits_counter.filter_trace_event_hit_count);
  EXPECT_EQ(1u, filter_hits_counter.end_event_hit_count);
  Clear();
}

// Flaky on iOS device, see crbug.com/908002
#if defined(OS_IOS) && !(TARGET_OS_SIMULATOR)
#define MAYBE_EventFiltering DISABLED_EventFiltering
#else
#define MAYBE_EventFiltering EventFiltering
#endif  // defined(OS_IOS) && !(TARGET_OS_SIMULATOR)
TEST_F(TraceEventTestFixture, MAYBE_EventFiltering) {
  const char config_json[] =
      "{"
      "  \"included_categories\": ["
      "    \"test_filtered_cat\","
      "    \"test_unfiltered_cat\","
      "    \"" TRACE_DISABLED_BY_DEFAULT("test_filtered_cat") "\","
      "    \"" TRACE_DISABLED_BY_DEFAULT("test_unfiltered_cat") "\"],"
      "  \"event_filters\": ["
      "     {"
      "       \"filter_predicate\": \"testing_predicate\", "
      "       \"included_categories\": ["
      "         \"test_filtered_cat\","
      "         \"" TRACE_DISABLED_BY_DEFAULT("test_filtered_cat") "\"]"
      "     }"
      "    "
      "  ]"
      "}";

  TestEventFilter::HitsCounter filter_hits_counter;
  TestEventFilter::set_filter_return_value(true);
  TraceLog::GetInstance()->SetFilterFactoryForTesting(TestEventFilter::Factory);

  TraceConfig trace_config(config_json);
  TraceLog::GetInstance()->SetEnabled(
      trace_config, TraceLog::RECORDING_MODE | TraceLog::FILTERING_MODE);
  ASSERT_TRUE(TraceLog::GetInstance()->IsEnabled());

  TRACE_EVENT0("test_filtered_cat", "a snake");
  TRACE_EVENT0("test_filtered_cat", "a mushroom");
  TRACE_EVENT0("test_unfiltered_cat", "a horse");

  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("test_filtered_cat"), "a dog");
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("test_unfiltered_cat"), "a pony");

  // This is scoped so we can test the end event being filtered.
  { TRACE_EVENT0("test_filtered_cat", "another cat whoa"); }

  EndTraceAndFlush();

  EXPECT_EQ(4u, filter_hits_counter.filter_trace_event_hit_count);
  EXPECT_EQ(1u, filter_hits_counter.end_event_hit_count);
}

// Flaky on iOS device, see crbug.com/908002
#if defined(OS_IOS) && !(TARGET_OS_SIMULATOR)
#define MAYBE_EventAllowlistFiltering DISABLED_EventAllowlistFiltering
#else
#define MAYBE_EventAllowlistFiltering EventAllowlistFiltering
#endif  // defined(OS_IOS) && !(TARGET_OS_SIMULATOR)
TEST_F(TraceEventTestFixture, MAYBE_EventAllowlistFiltering) {
  std::string config_json = StringPrintf(
      "{"
      "  \"included_categories\": ["
      "    \"test_filtered_cat\","
      "    \"test_unfiltered_cat\","
      "    \"" TRACE_DISABLED_BY_DEFAULT("test_filtered_cat") "\"],"
      "  \"event_filters\": ["
      "     {"
      "       \"filter_predicate\": \"%s\", "
      "       \"included_categories\": ["
      "         \"test_filtered_cat\","
      "         \"" TRACE_DISABLED_BY_DEFAULT("*") "\"], "
      "       \"filter_args\": {"
      "           \"event_name_allowlist\": [\"a snake\", \"a dog\"]"
      "         }"
      "     }"
      "    "
      "  ]"
      "}",
      EventNameFilter::kName);

  TraceConfig trace_config(config_json);
  TraceLog::GetInstance()->SetEnabled(
      trace_config, TraceLog::RECORDING_MODE | TraceLog::FILTERING_MODE);
  EXPECT_TRUE(TraceLog::GetInstance()->IsEnabled());

  TRACE_EVENT0("test_filtered_cat", "a snake");
  TRACE_EVENT0("test_filtered_cat", "a mushroom");
  TRACE_EVENT0("test_unfiltered_cat", "a cat");
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("test_filtered_cat"), "a dog");
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("test_filtered_cat"), "a pony");

  EndTraceAndFlush();

  EXPECT_TRUE(FindMatchingValue("name", "a snake"));
  EXPECT_FALSE(FindMatchingValue("name", "a mushroom"));
  EXPECT_TRUE(FindMatchingValue("name", "a cat"));
  EXPECT_TRUE(FindMatchingValue("name", "a dog"));
  EXPECT_FALSE(FindMatchingValue("name", "a pony"));
}
#endif  // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)

TEST_F(TraceEventTestFixture, ClockSyncEventsAreAlwaysAddedToTrace) {
  BeginSpecificTrace("-*");
  TRACE_EVENT_CLOCK_SYNC_RECEIVER(1);
  EndTraceAndFlush();
  EXPECT_TRUE(FindNamePhase("clock_sync", "c"));
}

TEST_F(TraceEventTestFixture, ContextLambda) {
  TraceLog::GetInstance()->SetEnabled(TraceConfig(kRecordAllCategoryFilter, ""),
                                      TraceLog::RECORDING_MODE);

  {
    TRACE_EVENT1("cat", "Name", "arg", [&](perfetto::TracedValue ctx) {
      std::move(ctx).WriteString("foobar");
    });
  }
  EndTraceAndFlush();

  const Value* dict = FindNamePhase("Name", "X");
  ASSERT_TRUE(dict);

  const Value* args_dict = dict->FindDictKey("args");
  ASSERT_TRUE(args_dict);

  EXPECT_EQ(*args_dict->FindStringKey("arg"),
            "Unsupported (crbug.com/1225176)");
}

}  // namespace trace_event
}  // namespace base
