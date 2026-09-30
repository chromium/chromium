// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_ETW_ALLOCATION_OBSERVER_WIN_H_
#define BASE_ALLOCATOR_ETW_ALLOCATION_OBSERVER_WIN_H_

#include <stdint.h>

#include <atomic>
#include <string_view>

#include "base/allocator/dispatcher/notification_data.h"
#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "base/no_destructor.h"
#include "base/trace_event/trace_logging_minimal_win.h"

namespace base::allocator {

// The provider contract, kept at namespace scope rather than inside the class
// below on purpose: in component builds BASE_EXPORT makes class members
// dllimport, and the address of a dllimport constant is not a constant
// expression, so `constexpr std::string_view name = Class::kName;` would fail
// to compile for every consumer. These are part of the contract external tools
// bind to, so they have to stay usable in constant expressions.

// Provider name and GUID are a tightly bound pair. The GUID is the standard
// TraceLogging hash of the (upper-cased, UTF-16BE) name, so tools may resolve
// the provider either way. Changing one requires changing the other.
inline constexpr char kEtwProviderName[] = "Chromium.AllocationTracing";

// {C9D77D7A-5129-51F6-3C45-3E0FD0418284}
inline constexpr GUID kEtwProviderGuid = {
    0xC9D77D7A,
    0x5129,
    0x51F6,
    {0x3C, 0x45, 0x3E, 0x0F, 0xD0, 0x41, 0x82, 0x84}};

// Keyword bits. A session picks what it wants to pay for, e.g.
//   xperf -start alloc -on C9D77D7A-5129-51F6-3C45-3E0FD0418284:0x3:5'stack
// Note that a session must request at least one keyword: enabling with a
// MatchAnyKeyword of 0 leaves the provider silent.
inline constexpr uint64_t kEtwAllocationKeyword = 0x1;
inline constexpr uint64_t kEtwFreeKeyword = 0x2;

// Process type keyword bits. These select which processes write events rather
// than which events get written: a session that sets none of them traces every
// process, and a session that sets any of them traces only processes of those
// types. For example, 0x23 traces allocations and frees in renderers only, and
// 0x53 in the browser and GPU processes only. Events themselves only ever carry
// kEtwAllocationKeyword or kEtwFreeKeyword.
//
// The selection is made inside each process against the union of the keywords
// of every session that enabled the provider, so it scopes which processes
// write events but not which session receives them. Concurrent sessions
// therefore cannot select different processes, and a session sees events from
// every process the union selects rather than only the ones it asked for: if
// one session asks for frees in renderers while another asks for allocations in
// the browser, both see both. ETW cannot narrow this either, because
// MatchAnyKeyword is an or-match, so tagging events with their process type
// would not keep them out of the other session. A session that needs a hard
// guarantee has to scope itself with a process id filter.
//
// Exactly one of these describes any given process, which is why this is an
// enum and not just more keyword constants: a single process's bit and a
// session's keyword mask are both uint64_t but mean very different things, and
// confusing the two would silently change which processes get traced.
//
// The set is deliberately coarse and is not stable across Chromium versions.
// Existing bits keep their values, because command lines and profiles outside
// the tree already name them, but a later version may split a broad type into
// more specific ones. A process that reports kUtility or kOther today may
// report a narrower new bit instead once that happens, so a session that wants
// to keep tracing it has to add the new bit. Pick the types to trace
// explicitly rather than treating this list as exhaustive.
enum class EtwProcessTypeKeyword : uint64_t {
  kBrowser = 0x10,
  kRenderer = 0x20,
  kGpu = 0x40,
  kUtility = 0x80,
  // Any process type not named above.
  kOther = 0x100,
};

// Every EtwProcessTypeKeyword, so that splitting a type out later only takes
// an edit here rather than one in each place that has to cover them all.
inline constexpr EtwProcessTypeKeyword kAllEtwProcessTypes[] = {
    EtwProcessTypeKeyword::kBrowser, EtwProcessTypeKeyword::kRenderer,
    EtwProcessTypeKeyword::kGpu, EtwProcessTypeKeyword::kUtility,
    EtwProcessTypeKeyword::kOther};

// The above or'ed together, for masking off the process type bits of a
// session's keywords.
inline constexpr uint64_t kEtwProcessTypeKeywords = [] {
  uint64_t keywords = 0;
  for (const EtwProcessTypeKeyword process_type : kAllEtwProcessTypes) {
    keywords |= static_cast<uint64_t>(process_type);
  }
  return keywords;
}();

// Returns the process type of a process launched with --type=`process_type`.
// The browser process has an empty type.
BASE_EXPORT EtwProcessTypeKeyword
GetEtwProcessTypeKeyword(std::string_view process_type);

// TraceLogging event names, which double as the handles for
// EVENT_FILTER_TYPE_STACKWALK_NAME. "Alloc"/"Free" come from the allocator
// shim (malloc/new) and "PAAlloc"/"PAFree" from direct PartitionAlloc use.
//
// Every event carries "Addr", and the two allocation events also carry "Size".
// "PAAlloc" carries a third field, "Type", holding the name of the
// PartitionAlloc partition the memory came from, which is empty for untyped
// allocations. The shim's "Alloc" has no "Type" field at all, so consumers must
// key the payload layout off the event name.
inline constexpr char kEtwAllocationEventName[] = "Alloc";
inline constexpr char kEtwFreeEventName[] = "Free";
inline constexpr char kEtwPartitionAllocationEventName[] = "PAAlloc";
inline constexpr char kEtwPartitionFreeEventName[] = "PAFree";

// Emits an ETW event for every allocation and free reported by the allocator
// dispatcher, covering both the allocator shim (malloc/new, backed by
// PartitionAlloc-Everywhere) and direct PartitionAlloc usage. This lets
// out-of-process profilers attribute heap memory to call stacks the same way
// they do for the Windows heap provider, which sees almost nothing in Chromium
// because PartitionAlloc satisfies allocations out of its own super pages.
//
// Call stacks are not captured in-process. The tracing session is expected to
// enable the provider with EVENT_ENABLE_PROPERTY_STACK_TRACE, which makes the
// kernel walk the stack at event-write time. This works in sandboxed child
// processes: writing ETW events requires no handles, files, registry or ALPC
// access, only starting a session does (and that happens in the profiler).
//
// Stack walking is the expensive part of a capture and a free's stack is not
// the interesting one, so sessions should restrict stacks to the allocation
// events. Because TraceLogging events carry no static event ids, the id-based
// EVENT_FILTER_TYPE_STACKWALK filter is ignored for this provider; the event
// names below are the handles to use with EVENT_FILTER_TYPE_STACKWALK_NAME
// (Windows 10 1709 and later) instead. See the ready-made WPR profile in
// tools/win/etw_allocation_tracing.
//
// This is an allocator dispatcher observer, so it runs on every allocation and
// every free in the process and must never allocate. Only Register() allocates,
// which is why it has to be called before the observer is connected to the
// dispatcher.
//
// When no session is listening, the cost per allocation is a single relaxed
// atomic load and a well-predicted branch. The same holds in processes that the
// sessions' process type keywords leave out.
class BASE_EXPORT EtwAllocationObserver {
 public:
  // Returns the process-wide instance. It is intentionally never destroyed:
  // allocator hooks cannot be removed reliably, so the observer has to outlive
  // everything that might still be allocating during shutdown.
  static EtwAllocationObserver* Get();

  EtwAllocationObserver(const EtwAllocationObserver&) = delete;
  EtwAllocationObserver& operator=(const EtwAllocationObserver&) = delete;

  // Registers the ETW provider for a process launched with
  // --type=`process_type`, which decides the process type keyword this process
  // answers to. This allocates, so it must run before the observer is handed to
  // the allocator dispatcher. Calling it twice is fatal.
  void Register(std::string_view process_type);

  // Allocator dispatcher observer interface. Kept in the header so that the
  // early-out is inlined into the dispatcher's hooks.
  ALWAYS_INLINE void OnAllocation(
      const dispatcher::AllocationNotificationData& notification_data) {
    if ((enabled_keywords_.load(std::memory_order_relaxed) &
         kEtwAllocationKeyword) != 0) [[unlikely]] {
      WriteAllocationEvent(notification_data);
    }
  }

  ALWAYS_INLINE void OnFree(
      const dispatcher::FreeNotificationData& notification_data) {
    if ((enabled_keywords_.load(std::memory_order_relaxed) & kEtwFreeKeyword) !=
        0) [[unlikely]] {
      WriteFreeEvent(notification_data);
    }
  }

 private:
  friend class base::NoDestructor<EtwAllocationObserver>;
  friend class EtwAllocationObserverTest;

  EtwAllocationObserver();
  ~EtwAllocationObserver();

  // Forces the keywords that gate event writing, so that tests can exercise the
  // writing path without an elevated tracing session.
  void SetEnabledKeywordsForTesting(uint64_t keywords);

  // Called by ETW on a callback thread whenever a session enables or disables
  // the provider. It may run while other threads are inside an allocator hook,
  // so it must do nothing beyond publishing `enabled_keywords_`.
  void OnProviderStateChanged(TlmProvider::EventControlCode event);

  // Returns `keywords` if they select `process_type`, and 0 if they select
  // other process types only.
  static uint64_t FilterKeywordsForProcessType(
      uint64_t keywords,
      EtwProcessTypeKeyword process_type);

  // Out of line, and never inlined, to keep the enabled check cheap.
  NOINLINE void WriteAllocationEvent(
      const dispatcher::AllocationNotificationData& notification_data);
  NOINLINE void WriteFreeEvent(
      const dispatcher::FreeNotificationData& notification_data);

  // Keywords requested by the active sessions, or 0 while the provider is
  // disabled or the sessions select other process types only. This is the only
  // state the allocation path reads.
  std::atomic<uint64_t> enabled_keywords_{0};

  // This process's type. Set by Register() before the provider can be enabled.
  EtwProcessTypeKeyword process_type_ = EtwProcessTypeKeyword::kOther;

  TlmProvider provider_;
};

}  // namespace base::allocator

#endif  // BASE_ALLOCATOR_ETW_ALLOCATION_OBSERVER_WIN_H_
