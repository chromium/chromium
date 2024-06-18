// Copyright (c) 2021 Record Replay Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// API for interacting with the record/replay driver.

#ifndef BASE_RECORD_REPLAY_H_
#define BASE_RECORD_REPLAY_H_

#include "base/check.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"

#include "v8/include/replayio-macros.h"

#include <cstdint>
#include <memory>

namespace base {
  class DictionaryValue;
}

namespace recordreplay {

bool IsRecordingOrReplaying(const char* feature = nullptr,
                            const char* subfeature = nullptr);
bool IsRecording();
bool IsReplaying();
char* GetRecordingId();
void FinishRecording();

void Print(const char* format, ...);
void Diagnostic(const char* format, ...);
void CommandDiagnostic(const char* format, ...);
void CommandDiagnosticTrace(const char* format, ...);
void Warning(const char* format, ...);
void Trace(const char* format, ...);

bool HadMismatch();
bool HasAsserts();
void Assert(const char* format, ...);
void AssertMaybeEventsDisallowed(const char* format, ...);
void AssertBytes(const char* why, const void* buf, size_t size);
bool AreAssertsDisabled();

uintptr_t RecordReplayValue(const char* why, uintptr_t v);
void RecordReplayBytes(const char* why, void* buf, size_t size);
void RecordReplayString(const char* why, std::string& text);

int CreateOrderedLock(const char* name);
void OrderedLock(int lock);
void OrderedUnlock(int lock);

struct AutoOrderedLock {
  AutoOrderedLock(int id) : id_(id) { OrderedLock(id_); }
  ~AutoOrderedLock() { OrderedUnlock(id_); }
  int id_;
};

void InvalidateRecording(const char* why);
void NewCheckpoint();

uint64_t NewBookmark();
void OnAnnotation(const char* kind, const char* contents);
void OnNetworkRequest(const char* id, const char* kind, uint64_t bookmark);
void OnNetworkRequestEvent(const char* id);
void OnNetworkStreamStart(const char* id, const char* kind, const char* parentId);
void OnNetworkStreamData(const char* id, size_t offset, size_t length, uint64_t bookmark);
void OnNetworkStreamEnd(const char* id, size_t length);

void BeginPassThroughEvents();
void EndPassThroughEvents();
bool AreEventsPassedThrough(const char* why = nullptr);

void BeginDisallowEvents();
void BeginDisallowEventsWithLabel(const char* label);

void EndDisallowEvents();

// Whether we are in a code path that we have explicitely identified as
// divergent.
bool AreEventsDisallowed(const char* why = nullptr);

void EnterReplayCode();
void ExitReplayCode();

bool FeatureEnabled(const char* feature, const char* subfeature = nullptr);
bool HasDisabledFeatures();

/**
 * Get the current JS stack, if there is any.
 */
void GetCurrentJSStack(std::string* stackTrace);

void BrowserEvent(const char* msg, const base::DictionaryValue& info);

struct AutoPassThroughEvents {
  AutoPassThroughEvents() { BeginPassThroughEvents(); }
  ~AutoPassThroughEvents() { EndPassThroughEvents(); }
};

struct AutoDisallowEvents {
  AutoDisallowEvents(const char* label) { BeginDisallowEventsWithLabel(label); }
  ~AutoDisallowEvents() { EndDisallowEvents(); }
};

// Whether we have intentionally diverged from recording at a pause point.
// After diverging, we:
// 1. Cannot interact with the recording stream any longer.
// 2. Are guaranteed that no runToPoint will follow.
// 
// The above two constraints allow us to execute arbitrary code without fear
// of causing downstream divergences.
bool HasDivergedFromRecording();
bool AllowSideEffects();

void RegisterPointer(const char* name, const void* ptr);
void UnregisterPointer(const void* ptr);
int PointerId(const void* ptr);
void* IdPointer(int id);

void OnEvent(const char* aEvent, bool aBefore);
void OnMouseEvent(const char* kind, size_t clientX, size_t clientY, bool synthetic);
void OnKeyEvent(const char* kind, const char* key, bool synthetic);
void OnNavigationEvent(const char* kind, const char* url);

// Create new identifiers, as with RegisterPointer/PointerId but with less
// overhead and requiring manual storage.
int NewIdMainThread(const char* name);
int NewIdAnyThread(const char* name);

// Crash instantly with given reason.
void Crash(const char* format, ...);

// Return whether record/replay specific scripts are executing.
bool IsInReplayCode(const char* why = nullptr);

// Return whether we are in a divergent code segment where we cannot
// read/write the recording stream.
bool AreEventsUnavailable(const char* why = nullptr);


// Mark a region where record/replay specific scripts are executing.
struct AutoMarkReplayCode {
  AutoMarkReplayCode();
  ~AutoMarkReplayCode();
};

// stl comparator that uses pointer IDs to compare elements when recording/replaying,
// giving a deterministic sort order.
struct CompareByPointerId {
  template <typename T>
  bool operator()(const T* a, const T* b) const {
    if (IsRecordingOrReplaying("pointer-ids")) {
      int ida = PointerId(a);
      int idb = PointerId(b);
      CHECK(ida && idb);
      return ida < idb;
    }
    return (uintptr_t)a < (uintptr_t)b;
  }
};

// stl comparator for classes that have a RecordReplayId() method.
struct CompareByRecordReplayId {
  template <typename T>
  bool operator()(const T* a, const T* b) const {
    if (IsRecordingOrReplaying("pointer-ids")) {
      int ida = a->RecordReplayId();
      int idb = b->RecordReplayId();
      CHECK(ida && idb);
      return ida < idb;
    }
    return (uintptr_t)a < (uintptr_t)b;
  }
};

// For use with scoped_refptr and similar.
template <typename T>
struct CompareRefptrByPointerId {
  bool operator()(const T& a, const T& b) const {
    if (IsRecordingOrReplaying("pointer-ids")) {
      int ida = PointerId(a.get());
      int idb = PointerId(b.get());
      CHECK(ida && idb);
      return ida < idb;
    }
    return a < b;
  }
};

// For use with scoped_refptr and similar.
template <typename T>
struct CompareRefptrByRecordReplayId {
  bool operator()(const T& a, const T& b) const {
    if (IsRecordingOrReplaying("pointer-ids")) {
      int ida = a.get()->RecordReplayId();
      int idb = b.get()->RecordReplayId();
      CHECK(ida && idb);
      return ida < idb;
    }
    return a < b;
  }
};

// For use with blink WeakMember and Member.
template <typename T>
struct CompareMemberByPointerId {
  bool operator()(const T& a, const T& b) const {
    if (IsRecordingOrReplaying("pointer-ids")) {
      int ida = PointerId(a.Get());
      int idb = PointerId(b.Get());
      CHECK(ida && idb);
      return ida < idb;
    }
    return a < b;
  }
};

inline unsigned HashInt(uint32_t key) {
  key += ~(key << 15);
  key ^= (key >> 10);
  key += (key << 3);
  key ^= (key >> 6);
  key += ~(key << 11);
  key ^= (key >> 16);
  return key;
}

inline unsigned HashInt(uint64_t key) {
  key += ~(key << 32);
  key ^= (key >> 22);
  key += ~(key << 13);
  key ^= (key >> 8);
  key += (key << 3);
  key ^= (key >> 15);
  key += ~(key << 27);
  key ^= (key >> 31);
  return static_cast<unsigned>(key);
}

// Replay's hashing function for pointers, using the registered pointer id.
template <typename T>
struct ReplayPointerIdHash {
  static unsigned GetHash(T* key) {
    if (!IsRecordingOrReplaying("pointer-ids")) {
      return HashInt((uint64_t)key);
    }

    int ptr = PointerId(key);
    CHECK(ptr != 0);
    return HashInt((uint32_t)ptr);
  }
  static bool Equal(T* a, T* b) { return a == b; }
  static bool Equal(std::nullptr_t, T* b) { return !b; }
  static bool Equal(T* a, std::nullptr_t) { return !a; }
  static const bool safe_to_compare_to_empty_or_deleted = true;
};

// Replay's hashing function for scoped pointers, using the registered pointer id.
template <typename T>
struct ReplayRefPointerIdHash : ReplayPointerIdHash<T> {
  using ReplayPointerIdHash<T>::GetHash;
  static unsigned GetHash(const scoped_refptr<T>& key) {
    if (!IsRecordingOrReplaying("pointer-ids")) {
      return HashInt((uint64_t)key.get());
    }

    int ptr = PointerId(key.get());
    CHECK(ptr != 0);
    return HashInt((uint32_t)ptr);
  }
  using ReplayPointerIdHash<T>::Equal;
  static bool Equal(const scoped_refptr<T>& a, const scoped_refptr<T>& b) {
    return a == b;
  }
  static bool Equal(T* a, const scoped_refptr<T>& b) { return a == b; }
  static bool Equal(const scoped_refptr<T>& a, T* b) { return a == b; }
};


// For taking ordered locks when events might be disallowed. Passes through
// events during the acquire to avoid generating a warning.
class SCOPED_LOCKABLE AutoLockMaybeEventsDisallowed {
 public:
  AutoLockMaybeEventsDisallowed(base::Lock& lock) EXCLUSIVE_LOCK_FUNCTION(lock);
  ~AutoLockMaybeEventsDisallowed() UNLOCK_FUNCTION();

 private:
  base::Lock& lock_;
};

// For releasing ordered locks when events might be disallowed. Passes through
// events during the acquire to avoid generating a warning.
class SCOPED_LOCKABLE AutoUnlockMaybeEventsDisallowed {
 public:
  AutoUnlockMaybeEventsDisallowed(base::Lock& lock);
  ~AutoUnlockMaybeEventsDisallowed();

 private:
  base::Lock& lock_;
};

// Returns whether dependency graph APIs need to be called.
bool DependencyGraphEnabled();

int NewDependencyGraphNode(const char* json);
void AddDependencyGraphEdge(int source, int target, const char* json);
void BeginDependencyExecution(int node);
void EndDependencyExecution();

struct AutoDependencyExecution {
  AutoDependencyExecution(int node) {
    BeginDependencyExecution(node);
  }
  ~AutoDependencyExecution() {
    EndDependencyExecution();
  }
};

// Helper class which creates a marker dependency node to associate with this
// region of execution. Marker nodes are general purpose for noting dependencies
// that are either uninteresting for dependency analysis or where specifying
// more specific JSON for the node is not yet implemented.
struct AutoMarkerDependencyExecution {
  // |reason| is a well known string describing why we're marking this region,
  // for example ScriptExecution for regions that can execute script or
  // LoadEventDelay for regions that can remove a load event delay from a document.
  //
  // |name| is the function whose execution region is being marked.
  AutoMarkerDependencyExecution(const char* reason, const char* name);
  ~AutoMarkerDependencyExecution();
};

// RAII class to enable recording assertions on dynamic-length buffer 
// allocations. Used to track down the allocation causing mismatched message 
// sizes when replaying.
// TODO: Merge this with the similar `AutoRecordReplayAssertBufferAllocations`
// in mojo/public/cpp/bindings/lib/buffer.cc (for main-thread only).
struct AutoAssertBufferAllocations {
  AutoAssertBufferAllocations(const char* issueLabel = "");
  ~AutoAssertBufferAllocations();
};

// Utility macro to add RecordReplayId to a class.
// To be used in conjunction with INIT_RECORD_REPLAY_ID.
#define HAS_RECORD_REPLAY_ID() \
 private:                      \
  int record_replay_id_ = 0;   \
                               \
 public:                       \
  int RecordReplayId() const { \
    return record_replay_id_;  \
  }                            \
  static_assert(true, "require semicolon")

// Utility macro to initialize record_replay_id_ inside the ctor of a class of
// given name.
#define INIT_RECORD_REPLAY_ID(name) \
  record_replay_id_ = recordreplay::NewIdAnyThread(#name)

// Utility macro for optimized initialization of record_replay_id_ inside the
// ctor of a class of given name, in case its only used on the main thread.
#define INIT_RECORD_REPLAY_ID_MAIN_THREAD(name) \
  record_replay_id_ = recordreplay::NewIdMainThread(#name)


// This drop-in replacement for unique_ptr purposefully leaks owned memory
// in non-deterministic execution paths, so as not to perform cleanup operations
// that require deterministic execution.
// First discussed here:
// https://linear.app/replay/issue/RUN-1227/divergence-frameschedulerimpl-destroys-powermodevoter
// Playground: https://replit.com/@Domiii/Leak-Unique-Ptr#main.cpp
template <typename T>
class unique_leaky_ptr {
  std::unique_ptr<T> p;

public:
  using pointer = T*;
 
  // copy ctor
  template <typename T_>
  unique_leaky_ptr(T_&) = delete;
 
  // copy assignment
  template <typename T_>
  unique_leaky_ptr& operator=(const T_&) = delete;
 
  // move ctors
  unique_leaky_ptr(unique_leaky_ptr&& u) : unique_leaky_ptr(std::move(u.p)) {}
  template <typename T_>
  unique_leaky_ptr(T_&& u) : p(std::move(u)) {}
 
  // move assignments
  unique_leaky_ptr& operator=(unique_leaky_ptr&& u) noexcept {
    p = std::move(u.p);
  }
  template <typename T_>
  unique_leaky_ptr& operator=(T_&& val) noexcept {
    p = val;
    return *this;
  }

  // Return the stored pointer.
  pointer operator->() const noexcept { return get(); }
  pointer get() const noexcept { return p.get(); }

  // Return @c true if the stored pointer is not null.
  explicit operator bool() const noexcept {
    return !!p;
  }

  // Release ownership of any stored pointer.
  pointer release() noexcept { return p.release(); }

  // dtor
  ~unique_leaky_ptr() {
    if (AreEventsDisallowed("unique_leaky_ptr")) {
      // Leak the allocated memory before destructing `unique_ptr`
      // when inside a non-deterministic execution path.
      p.release();
    }
  }
};

/*
 * A Pseudo-stack mechanism for diagnostics.
 *
 * Try to use the `AutoPseudoStackEntry` RAII class to interface with this.
 * 
 * Usage:
 * ```
 *    // Push an entry onto the stack, it'll get popped when `entry` is destroyed.
 *    recordreplay::AutoPseudoStackEntry entry("MyString");
 *
 *    ...
 *    // At any other point in the code...
 *
 *    // Iterate over the current entries in the pseudo-stack (outermost to innermost)
 *    for (const std::string& entry : recordreplay::ReadPseudoStack()) {
 *      ...
 *    }
 * 
 *    // Get a convenient string of the form `Foo => Bar => Baz` of the current
 *    // pseudostack entries.
 *    std::string cur_stack = recordreplay::ReadPseudoStackString();
 * ```
 */
size_t PushPseudoStackFrame(const char* name);
void PopPseudoStackFrame(const char* name, size_t id);
const std::vector<std::string>& ReadPseudoStack();
std::string ReadPseudoStackString();

class AutoPseudoStackEntry {
 public:
  AutoPseudoStackEntry(const char* name)
    : name_(name), id_(PushPseudoStackFrame(name)) {}
  ~AutoPseudoStackEntry() { PopPseudoStackFrame(name_, id_); }
  
 private:
  const char* name_;
  size_t id_;
};

} // namespace recordreplay

#endif // BASE_RECORD_REPLAY_H_
