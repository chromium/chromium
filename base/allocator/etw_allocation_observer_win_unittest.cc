// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/etw_allocation_observer_win.h"

#include <windows.h>

#include <evntcons.h>
#include <evntrace.h>
#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/allocator/dispatcher/dispatcher.h"
#include "base/allocator/dispatcher/notification_data.h"
#include "base/allocator/dispatcher/subsystem.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/process/process_handle.h"
#include "base/strings/string_number_conversions_win.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::allocator {

namespace {

// Returns the process ids that currently have `provider_guid` registered,
// straight from ETW. Used to prove the provider really made it into the
// kernel's registration table rather than just returning success.
std::vector<uint32_t> GetProviderRegistrationPids(const GUID& provider_guid) {
  std::vector<uint32_t> pids;
  std::vector<uint8_t> buffer(64 * 1024);
  ULONG bytes_written = 0;
  GUID guid = provider_guid;
  const ULONG status = ::EnumerateTraceGuidsEx(
      TraceGuidQueryInfo, &guid, sizeof(guid), buffer.data(),
      static_cast<ULONG>(buffer.size()), &bytes_written);
  if (status != ERROR_SUCCESS || bytes_written < sizeof(TRACE_GUID_INFO)) {
    return pids;
  }

  // ETW returns a TRACE_GUID_INFO header followed by a self-linked list of
  // TRACE_PROVIDER_INSTANCE_INFO records, one per process that has the
  // provider registered.
  span<const uint8_t> bytes = span(buffer).first(bytes_written);
  TRACE_GUID_INFO guid_info = {};
  byte_span_from_ref(guid_info).copy_from(bytes.first(sizeof(guid_info)));
  bytes = bytes.subspan(sizeof(guid_info));

  for (ULONG i = 0; i < guid_info.InstanceCount; ++i) {
    if (bytes.size() < sizeof(TRACE_PROVIDER_INSTANCE_INFO)) {
      break;
    }
    TRACE_PROVIDER_INSTANCE_INFO instance_info = {};
    byte_span_from_ref(instance_info)
        .copy_from(bytes.first(sizeof(instance_info)));
    pids.push_back(instance_info.Pid);
    if (instance_info.NextOffset == 0 ||
        instance_info.NextOffset > bytes.size()) {
      break;
    }
    bytes = bytes.subspan(instance_info.NextOffset);
  }
  return pids;
}

// The sentinel values the end-to-end test emits, chosen so they cannot be
// confused with a real allocation that happens to be traced concurrently. The
// address travels through a void* before the observer widens it back out, so it
// has to be declared pointer-sized to survive that round trip on 32-bit.
constexpr uintptr_t kSentinelAddress =
    static_cast<uintptr_t>(0xdeadbeef12345678ull);
constexpr size_t kSentinelSize = 0x1234u;

struct CaptureResult {
  int matching_allocs = 0;
  int matching_frees = 0;
  int other_events = 0;
};

// Serializes an EVENT_FILTER_EVENT_NAME payload: a fixed header followed by
// `names.size()` NUL-terminated UTF-8 event names packed back to back. This is
// the payload shared by EVENT_FILTER_TYPE_STACKWALK_NAME and
// EVENT_FILTER_TYPE_EVENT_NAME, the only filters that work against a
// TraceLogging provider, whose events have no static event ids.
std::vector<uint8_t> BuildEventNameFilterBlob(
    uint64_t match_any_keyword,
    uint8_t level,
    bool filter_in,
    span<const std::string_view> names) {
  constexpr size_t kHeaderSize = offsetof(EVENT_FILTER_EVENT_NAME, Names);
  size_t names_size = 0;
  for (std::string_view name : names) {
    names_size += name.size() + 1;
  }
  std::vector<uint8_t> blob(kHeaderSize + names_size, 0);

  EVENT_FILTER_EVENT_NAME header = {};
  header.MatchAnyKeyword = match_any_keyword;
  header.MatchAllKeyword = 0;
  header.Level = level;
  header.FilterIn = filter_in ? TRUE : FALSE;
  header.NameCount = static_cast<USHORT>(names.size());

  span<uint8_t> blob_span = span(blob);
  blob_span.first(kHeaderSize)
      .copy_from(
          byte_span_from_ref(allow_nonunique_obj, header).first(kHeaderSize));

  span<uint8_t> rest = blob_span.subspan(kHeaderSize);
  for (std::string_view name : names) {
    rest.first(name.size()).copy_from(as_byte_span(name));
    rest = rest.subspan(name.size() + 1);  // The NUL is already zeroed.
  }
  return blob;
}

// A private, in-process ETW session. Unlike a normal kernel-backed session this
// needs no administrator rights, which is exactly what makes it usable as a
// test: it proves the whole path from the observer through EventWrite() and
// back out of a real .etl file.
class ScopedPrivateEtwSession {
 public:
  ScopedPrivateEtwSession() = default;

  ScopedPrivateEtwSession(const ScopedPrivateEtwSession&) = delete;
  ScopedPrivateEtwSession& operator=(const ScopedPrivateEtwSession&) = delete;

  ~ScopedPrivateEtwSession() { Stop(); }

  // Returns the Win32 error code from StartTrace().
  ULONG Start(const FilePath& etl_path) {
    session_name_ =
        L"ChromiumAllocEtwTest-" + NumberToWString(GetCurrentProcId());
    const std::wstring& log_file = etl_path.value();
    CHECK_LT(log_file.size(), std::size(properties_.log_file_name));
    span(properties_.log_file_name).copy_prefix_from(log_file);

    properties_.props.Wnode.BufferSize = sizeof(properties_);
    properties_.props.Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    properties_.props.Wnode.ClientContext = 1;  // QPC timestamps.
    properties_.props.LogFileMode = EVENT_TRACE_PRIVATE_LOGGER_MODE |
                                    EVENT_TRACE_PRIVATE_IN_PROC |
                                    EVENT_TRACE_FILE_MODE_SEQUENTIAL;
    properties_.props.BufferSize = 64;
    properties_.props.MinimumBuffers = 4;
    properties_.props.MaximumBuffers = 16;
    properties_.props.LoggerNameOffset = offsetof(Properties, logger_name);
    properties_.props.LogFileNameOffset = offsetof(Properties, log_file_name);

    return ::StartTraceW(&handle_, session_name_.c_str(), &properties_.props);
  }

  // Enables the provider. If `stack_event_names` is non-empty, stack walking
  // is requested but restricted to those TraceLogging event names.
  //
  // Note that EVENT_FILTER_TYPE_STACKWALK (filtering by event id) is documented
  // to be ignored for TraceLogging providers, because TraceLogging events have
  // no static event ids. Filtering by name is the mechanism that works, and it
  // needs Windows 10 1709 or later.
  ULONG EnableProvider(const GUID& provider_guid,
                       uint64_t keywords,
                       span<const std::string_view> event_names = {},
                       ULONG filter_type = EVENT_FILTER_TYPE_STACKWALK_NAME,
                       bool request_stack_walking = false) {
    GUID guid = provider_guid;
    if (event_names.empty()) {
      return ::EnableTraceEx2(handle_, &guid,
                              EVENT_CONTROL_CODE_ENABLE_PROVIDER,
                              TRACE_LEVEL_VERBOSE, keywords, 0, 0, nullptr);
    }

    // EVENT_FILTER_EVENT_NAME is a variable length structure: a fixed header
    // followed by NameCount NUL-terminated UTF-8 event names packed back to
    // back.
    filter_blob_ = BuildEventNameFilterBlob(keywords, TRACE_LEVEL_VERBOSE,
                                            /*filter_in=*/true, event_names);

    EVENT_FILTER_DESCRIPTOR filter = {};
    filter.Ptr = reinterpret_cast<ULONGLONG>(filter_blob_.data());
    filter.Size = static_cast<ULONG>(filter_blob_.size());
    filter.Type = filter_type;

    ENABLE_TRACE_PARAMETERS parameters = {};
    parameters.Version = ENABLE_TRACE_PARAMETERS_VERSION_2;
    parameters.EnableProperty =
        request_stack_walking ? EVENT_ENABLE_PROPERTY_STACK_TRACE : 0;
    parameters.EnableFilterDesc = &filter;
    parameters.FilterDescCount = 1;

    return ::EnableTraceEx2(handle_, &guid, EVENT_CONTROL_CODE_ENABLE_PROVIDER,
                            TRACE_LEVEL_VERBOSE, keywords, 0, 0, &parameters);
  }

  // Stops the session, returning the Win32 error code from ControlTraceW().
  // The handle is kept if that fails, so the destructor can retry rather than
  // leaking a running session.
  ULONG Stop() {
    if (handle_ == 0) {
      return ERROR_SUCCESS;
    }
    Properties stop_properties = {};
    stop_properties.props.Wnode.BufferSize = sizeof(stop_properties);
    stop_properties.props.LoggerNameOffset = offsetof(Properties, logger_name);
    stop_properties.props.LogFileNameOffset =
        offsetof(Properties, log_file_name);
    const ULONG status = ::ControlTraceW(
        handle_, nullptr, &stop_properties.props, EVENT_TRACE_CONTROL_STOP);
    if (status != ERROR_SUCCESS) {
      return status;
    }
    // Only meaningful once the session has actually stopped, because the .etl
    // file is not complete until then.
    events_lost_ = stop_properties.props.EventsLost;
    handle_ = 0;
    return ERROR_SUCCESS;
  }

  // Only meaningful after Stop().
  ULONG events_lost() const { return events_lost_; }

 private:
  // ETW expects the logger and log file names to live in the same allocation as
  // the properties struct, at the declared offsets.
  struct Properties {
    EVENT_TRACE_PROPERTIES props;
    wchar_t logger_name[128];
    wchar_t log_file_name[MAX_PATH + 1];
  };

  TRACEHANDLE handle_ = 0;
  std::wstring session_name_;
  Properties properties_ = {};
  std::vector<uint8_t> filter_blob_;
  ULONG events_lost_ = 0;
};

void WINAPI OnEventRecord(EVENT_RECORD* record) {
  auto* result = static_cast<CaptureResult*>(record->UserContext);
  if (!IsEqualGUID(record->EventHeader.ProviderId, kEtwProviderGuid)) {
    return;
  }

  // TraceLogging packs the fields with no per-field headers, so the payload can
  // be read directly: "Alloc" is {uint64 Addr, uint64 Size} and "Free" is
  // {uint64 Addr}. Decoding it here doubles as a check that the events carry
  // the values the observer was handed.
  // SAFETY: ETW guarantees `UserData` points to `UserDataLength` valid bytes.
  const span<const uint8_t> payload =
      UNSAFE_BUFFERS(span(static_cast<const uint8_t*>(record->UserData),
                          size_t{record->UserDataLength}));
  const uint64_t keyword = record->EventHeader.EventDescriptor.Keyword;

  if (keyword == kEtwAllocationKeyword &&
      payload.size() == 2 * sizeof(uint64_t)) {
    uint64_t address = 0;
    uint64_t size = 0;
    byte_span_from_ref(address).copy_from(payload.first<sizeof(uint64_t)>());
    byte_span_from_ref(size).copy_from(
        payload.subspan<sizeof(uint64_t), sizeof(uint64_t)>());
    if (address == kSentinelAddress && size == kSentinelSize) {
      ++result->matching_allocs;
      return;
    }
  } else if (keyword == kEtwFreeKeyword && payload.size() == sizeof(uint64_t)) {
    uint64_t address = 0;
    byte_span_from_ref(address).copy_from(payload.first<sizeof(uint64_t)>());
    if (address == kSentinelAddress) {
      ++result->matching_frees;
      return;
    }
  }
  ++result->other_events;
}

// Replays `etl_path` through ETW's own consumer API and tallies the events the
// observer wrote.
CaptureResult ConsumeTrace(const FilePath& etl_path) {
  CaptureResult result;
  std::wstring log_file = etl_path.value();

  EVENT_TRACE_LOGFILEW logfile = {};
  logfile.LogFileName = log_file.data();
  logfile.ProcessTraceMode = PROCESS_TRACE_MODE_EVENT_RECORD;
  logfile.EventRecordCallback = &OnEventRecord;
  logfile.Context = &result;

  const TRACEHANDLE handle = ::OpenTraceW(&logfile);
  if (handle == INVALID_PROCESSTRACE_HANDLE) {
    ADD_FAILURE() << "OpenTrace failed: " << ::GetLastError();
    return result;
  }

  TRACEHANDLE handles[] = {handle};
  const ULONG status = ::ProcessTrace(span(handles).data(), std::size(handles),
                                      nullptr, nullptr);
  EXPECT_EQ(static_cast<ULONG>(ERROR_SUCCESS), status);
  // ProcessTrace() has returned, so the consumer is drained and this cannot be
  // the documented ERROR_CTX_CLOSE_PENDING.
  EXPECT_EQ(static_cast<ULONG>(ERROR_SUCCESS), ::CloseTrace(handle));
  return result;
}

}  // namespace

class EtwAllocationObserverTest : public ::testing::Test {
 public:
  static void SetUpTestSuite() {
    // The observer is a process-wide singleton and registering twice is fatal,
    // so register only once per process, as the browser process, even when the
    // suite is repeated.
    static bool registered = false;
    if (!registered) {
      EtwAllocationObserver::Get()->Register(/*process_type=*/"");
      registered = true;
    }
  }

 protected:
  void TearDown() override { observer()->SetEnabledKeywordsForTesting(0); }

  EtwAllocationObserver* observer() { return EtwAllocationObserver::Get(); }

  void SimulateProviderStateChange(TlmProvider::EventControlCode event) {
    observer()->OnProviderStateChanged(event);
  }

  uint64_t enabled_keywords() {
    return observer()->enabled_keywords_.load(std::memory_order_relaxed);
  }

  void SetEnabledKeywords(uint64_t keywords) {
    observer()->SetEnabledKeywordsForTesting(keywords);
  }

  static uint64_t FilterKeywordsForProcessType(
      uint64_t keywords,
      EtwProcessTypeKeyword process_type) {
    return EtwAllocationObserver::FilterKeywordsForProcessType(keywords,
                                                               process_type);
  }

  // Reports `count` allocation and free pairs of the sentinel, as the allocator
  // shim would.
  void EmitSentinelEventPairs(int count) {
    void* const address = reinterpret_cast<void*>(kSentinelAddress);
    for (int i = 0; i < count; ++i) {
      observer()->OnAllocation(dispatcher::AllocationNotificationData(
          address, kSentinelSize, nullptr,
          dispatcher::AllocationSubsystem::kAllocatorShim));
      observer()->OnFree(dispatcher::FreeNotificationData(
          address, dispatcher::AllocationSubsystem::kAllocatorShim));
    }
  }
};

// The provider must actually be registered with ETW, otherwise no session can
// ever turn tracing on.
TEST_F(EtwAllocationObserverTest, RegistersProviderWithEtw) {
  const std::vector<uint32_t> pids =
      GetProviderRegistrationPids(kEtwProviderGuid);
  EXPECT_NE(pids.end(), std::ranges::find(pids, GetCurrentProcId()))
      << "provider not registered for this process";
}

// While no session is listening the observer must stay completely silent.
TEST_F(EtwAllocationObserverTest, DisabledByDefault) {
  EXPECT_EQ(0u, enabled_keywords());
}

// Disabling the provider has to clear the keywords, otherwise the process would
// keep paying for the writing path after tracing stopped.
TEST_F(EtwAllocationObserverTest, ProviderStateChangeClearsKeywords) {
  SetEnabledKeywords(kEtwAllocationKeyword | kEtwFreeKeyword);
  ASSERT_NE(0u, enabled_keywords());

  SimulateProviderStateChange(TlmProvider::EventControlCode::kDisableProvider);
  EXPECT_EQ(0u, enabled_keywords());
}

// An enable callback publishes whatever keywords the sessions asked for. With
// no real session attached that is zero, which must leave the observer silent
// rather than writing everything.
TEST_F(EtwAllocationObserverTest, EnableWithoutSessionKeepsObserverSilent) {
  SimulateProviderStateChange(TlmProvider::EventControlCode::kEnableProvider);
  EXPECT_EQ(0u, enabled_keywords());
}

// The process type strings are duplicated from content, which base cannot
// depend on.
TEST_F(EtwAllocationObserverTest, MapsProcessTypesToKeywords) {
  EXPECT_EQ(EtwProcessTypeKeyword::kBrowser, GetEtwProcessTypeKeyword(""));
  EXPECT_EQ(EtwProcessTypeKeyword::kRenderer,
            GetEtwProcessTypeKeyword("renderer"));
  EXPECT_EQ(EtwProcessTypeKeyword::kGpu,
            GetEtwProcessTypeKeyword("gpu-process"));
  EXPECT_EQ(EtwProcessTypeKeyword::kUtility,
            GetEtwProcessTypeKeyword("utility"));
  EXPECT_EQ(EtwProcessTypeKeyword::kOther,
            GetEtwProcessTypeKeyword("crashpad-handler"));
}

// A session that sets no process type keyword has to keep tracing every
// process, while a session that sets some has to trace only those types.
TEST_F(EtwAllocationObserverTest, ProcessTypeKeywordsSelectProcesses) {
  constexpr uint64_t kEvents = kEtwAllocationKeyword | kEtwFreeKeyword;
  for (const EtwProcessTypeKeyword process_type : kAllEtwProcessTypes) {
    const uint64_t process = static_cast<uint64_t>(process_type);
    SCOPED_TRACE(process);
    EXPECT_EQ(kEvents, FilterKeywordsForProcessType(kEvents, process_type));
    EXPECT_EQ(kEvents | process,
              FilterKeywordsForProcessType(kEvents | process, process_type));
    EXPECT_EQ(kEvents | kEtwProcessTypeKeywords,
              FilterKeywordsForProcessType(kEvents | kEtwProcessTypeKeywords,
                                           process_type));
    EXPECT_EQ(
        0u, FilterKeywordsForProcessType(
                kEvents | (kEtwProcessTypeKeywords & ~process), process_type));
  }

  // The examples documented with the keywords.
  EXPECT_EQ(
      0u, FilterKeywordsForProcessType(0x23, EtwProcessTypeKeyword::kBrowser));
  EXPECT_EQ(0x23u, FilterKeywordsForProcessType(
                       0x23, EtwProcessTypeKeyword::kRenderer));
  EXPECT_EQ(0x53u, FilterKeywordsForProcessType(
                       0x53, EtwProcessTypeKeyword::kBrowser));
  EXPECT_EQ(0x53u,
            FilterKeywordsForProcessType(0x53, EtwProcessTypeKeyword::kGpu));
  EXPECT_EQ(
      0u, FilterKeywordsForProcessType(0x53, EtwProcessTypeKeyword::kRenderer));
}

// Exercises the actual notification path, including the re-entrancy guard, for
// both the allocator shim and the direct PartitionAlloc subsystem. Without a
// listening session TlmProvider::WriteEvent short-circuits, but everything up
// to and including the keyword checks runs for real. This would deadlock or
// recurse without bound if the writing path allocated.
TEST_F(EtwAllocationObserverTest, NotificationsAreSafeWhileEnabled) {
  SetEnabledKeywords(kEtwAllocationKeyword | kEtwFreeKeyword);

  int object = 0;
  for (int i = 0; i < 1000; ++i) {
    dispatcher::AllocationNotificationData shim_allocation(
        &object, sizeof(object), nullptr,
        dispatcher::AllocationSubsystem::kAllocatorShim);
    observer()->OnAllocation(shim_allocation);
    observer()->OnFree(dispatcher::FreeNotificationData(
        &object, dispatcher::AllocationSubsystem::kAllocatorShim));

    dispatcher::AllocationNotificationData pa_allocation(
        &object, sizeof(object), "TestPartition",
        dispatcher::AllocationSubsystem::kPartitionAllocator);
    observer()->OnAllocation(pa_allocation);
    observer()->OnFree(dispatcher::FreeNotificationData(
        &object, dispatcher::AllocationSubsystem::kPartitionAllocator));
  }

  // A null type name is legal for untyped PartitionAlloc allocations and must
  // not be handed to ETW as a null string pointer.
  dispatcher::AllocationNotificationData untyped(
      &object, sizeof(object), nullptr,
      dispatcher::AllocationSubsystem::kPartitionAllocator);
  observer()->OnAllocation(untyped);
}

// The observer has to be acceptable to the allocator dispatcher, and real
// allocations flowing through the installed hooks must not re-enter it.
TEST_F(EtwAllocationObserverTest, WorksAsAllocatorDispatcherObserver) {
  SetEnabledKeywords(kEtwAllocationKeyword | kEtwFreeKeyword);

  auto& dispatcher = dispatcher::Dispatcher::GetInstance();
  dispatcher.InitializeForTesting(observer());

  for (int i = 0; i < 1000; ++i) {
    auto data = std::make_unique<std::vector<int>>(64, i);
    EXPECT_EQ(64u, data->size());
  }

  dispatcher.ResetForTesting();
}

// The real thing: turn the provider on from an actual ETW session, emit
// allocations, and read them back out of the resulting .etl file. A private
// in-process session is used so the test needs no administrator rights.
TEST_F(EtwAllocationObserverTest, EmitsEventsToRealEtwSession) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const FilePath etl_path = temp_dir.GetPath().AppendASCII("alloc.etl");

  ScopedPrivateEtwSession session;
  const ULONG start_status = session.Start(etl_path);
  if (start_status != ERROR_SUCCESS) {
    GTEST_SKIP() << "could not start a private ETW session: " << start_status;
  }
  ASSERT_EQ(static_cast<ULONG>(ERROR_SUCCESS),
            session.EnableProvider(kEtwProviderGuid,
                                   kEtwAllocationKeyword | kEtwFreeKeyword));

  // EnableTraceEx2 runs the provider's enable callback synchronously, so the
  // observer must already know it is being traced.
  ASSERT_EQ(kEtwAllocationKeyword | kEtwFreeKeyword, enabled_keywords());

  constexpr int kEventPairs = 100;
  EmitSentinelEventPairs(kEventPairs);

  ASSERT_EQ(static_cast<ULONG>(ERROR_SUCCESS), session.Stop());

  // Stopping the session has to switch the observer back off.
  EXPECT_EQ(0u, enabled_keywords());

  const CaptureResult result = ConsumeTrace(etl_path);
  EXPECT_EQ(0u, session.events_lost());
  EXPECT_EQ(kEventPairs, result.matching_allocs);
  EXPECT_EQ(kEventPairs, result.matching_frees);
}

// Process type keywords have to hold against a real session, and enabling the
// provider again with different keywords has to redo the selection. The test
// process registered as the browser.
TEST_F(EtwAllocationObserverTest, ProcessTypeKeywordsApplyToRealEtwSession) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const FilePath etl_path = temp_dir.GetPath().AppendASCII("processes.etl");

  ScopedPrivateEtwSession session;
  const ULONG start_status = session.Start(etl_path);
  if (start_status != ERROR_SUCCESS) {
    GTEST_SKIP() << "could not start a private ETW session: " << start_status;
  }

  // Renderers only, so the browser must not write anything. The session would
  // record these events if they were written, since their keywords match.
  constexpr uint64_t kEvents = kEtwAllocationKeyword | kEtwFreeKeyword;
  constexpr uint64_t kRenderers =
      static_cast<uint64_t>(EtwProcessTypeKeyword::kRenderer);
  ASSERT_EQ(static_cast<ULONG>(ERROR_SUCCESS),
            session.EnableProvider(kEtwProviderGuid, kEvents | kRenderers));
  EXPECT_EQ(0u, enabled_keywords());
  EmitSentinelEventPairs(25);

  // The browser and renderers, so now it must.
  constexpr uint64_t kBrowserAndRenderers =
      kEvents | static_cast<uint64_t>(EtwProcessTypeKeyword::kBrowser) |
      kRenderers;
  ASSERT_EQ(static_cast<ULONG>(ERROR_SUCCESS),
            session.EnableProvider(kEtwProviderGuid, kBrowserAndRenderers));
  EXPECT_EQ(kBrowserAndRenderers, enabled_keywords());
  constexpr int kEventPairs = 100;
  EmitSentinelEventPairs(kEventPairs);

  ASSERT_EQ(static_cast<ULONG>(ERROR_SUCCESS), session.Stop());
  EXPECT_EQ(0u, enabled_keywords());

  const CaptureResult result = ConsumeTrace(etl_path);
  EXPECT_EQ(0u, session.events_lost());
  EXPECT_EQ(kEventPairs, result.matching_allocs);
  EXPECT_EQ(kEventPairs, result.matching_frees);
}

// Stack walking is by far the most expensive part of a capture, and a free's
// stack is not the interesting one anyway (the interesting stack is the one
// that allocated the memory), so the WPR profile in
// tools/win/etw_allocation_tracing restricts stacks to the allocation events by
// name. Because TraceLogging events have no static event ids,
// EVENT_FILTER_TYPE_STACKWALK is ignored for them and the name based
// EVENT_FILTER_TYPE_STACKWALK_NAME filter has to be used instead. Its payload
// is variable length, so pin the serialized layout down here.
TEST_F(EtwAllocationObserverTest, EventNameFilterBlobMatchesSdkLayout) {
  // Matches the StackEventNameFilters in chromium_allocation_tracing.wprp.
  static constexpr std::string_view kStackEventNames[] = {
      kEtwAllocationEventName, kEtwPartitionAllocationEventName};
  constexpr size_t kHeaderSize = offsetof(EVENT_FILTER_EVENT_NAME, Names);
  static_assert(kHeaderSize == 20, "unexpected EVENT_FILTER_EVENT_NAME header");

  const std::vector<uint8_t> blob =
      BuildEventNameFilterBlob(kEtwAllocationKeyword, TRACE_LEVEL_VERBOSE,
                               /*filter_in=*/true, kStackEventNames);

  // sizeof("Alloc") + sizeof("PAAlloc") worth of NUL-terminated names.
  ASSERT_EQ(kHeaderSize + 6u + 8u, blob.size());
  ASSERT_LE(blob.size(), size_t{MAX_EVENT_FILTER_EVENT_NAME_SIZE});

  EVENT_FILTER_EVENT_NAME header = {};
  byte_span_from_ref(allow_nonunique_obj, header)
      .first(kHeaderSize)
      .copy_from(span(blob).first(kHeaderSize));
  EXPECT_EQ(kEtwAllocationKeyword, header.MatchAnyKeyword);
  EXPECT_EQ(0u, header.MatchAllKeyword);
  EXPECT_EQ(TRACE_LEVEL_VERBOSE, header.Level);
  EXPECT_TRUE(header.FilterIn);
  EXPECT_EQ(2u, header.NameCount);

  static constexpr uint8_t kExpectedNames[] = {
      'A', 'l', 'l', 'o', 'c', '\0', 'P', 'A', 'A', 'l', 'l', 'o', 'c', '\0'};
  EXPECT_TRUE(
      std::ranges::equal(span(blob).subspan(kHeaderSize), kExpectedNames));
}

// Asking for name filtered stacks must never suppress the events themselves.
//
// Stack walking is by far the most expensive part of a capture, and a free's
// stack is not the interesting one, so the WPR profile restricts stacks to the
// allocation events with EVENT_FILTER_TYPE_STACKWALK_NAME. Restricting which
// events get a stack must not change which events are written.
//
// Note that a private in-process session rejects name based and stack filters
// with ERROR_INVALID_PARAMETER. That is a session capability limit rather than
// a payload problem: on the very same session a PID scope filter is accepted,
// while the fixed-size EVENT_FILTER_TYPE_STACKWALK_LEVEL_KW filter - whose
// payload cannot be malformed - is rejected as well. Real stack walking needs
// a kernel backed session, which needs administrator rights, so on such a
// session this skips rather than quietly re-enabling without the filter and
// asserting something it does not claim to test. The payload layout is covered
// by EventNameFilterBlobMatchesSdkLayout, unfiltered event emission by
// EmitsEventsToRealEtwSession, and the filtered path end to end by the WPR
// profile in tools/win/etw_allocation_tracing.
TEST_F(EtwAllocationObserverTest, StackWalkNameFilterDoesNotSuppressEvents) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const FilePath etl_path = temp_dir.GetPath().AppendASCII("filtered.etl");

  ScopedPrivateEtwSession session;
  const ULONG start_status = session.Start(etl_path);
  if (start_status != ERROR_SUCCESS) {
    GTEST_SKIP() << "could not start a private ETW session: " << start_status;
  }

  static constexpr std::string_view kStackEventNames[] = {
      kEtwAllocationEventName, kEtwPartitionAllocationEventName};
  const uint64_t keywords = kEtwAllocationKeyword | kEtwFreeKeyword;
  const ULONG enable_status = session.EnableProvider(
      kEtwProviderGuid, keywords, kStackEventNames,
      EVENT_FILTER_TYPE_STACKWALK_NAME, /*request_stack_walking=*/true);
  if (enable_status != ERROR_SUCCESS) {
    GTEST_SKIP() << "this session cannot filter stacks by event name: "
                 << enable_status;
  }
  ASSERT_NE(0u, enabled_keywords());

  constexpr int kEventPairs = 50;
  EmitSentinelEventPairs(kEventPairs);

  ASSERT_EQ(static_cast<ULONG>(ERROR_SUCCESS), session.Stop());

  const CaptureResult result = ConsumeTrace(etl_path);
  EXPECT_EQ(kEventPairs, result.matching_allocs);
  EXPECT_EQ(kEventPairs, result.matching_frees)
      << "restricting stack walking must not drop the free events";
}

}  // namespace base::allocator
