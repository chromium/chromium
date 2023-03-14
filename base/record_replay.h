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

#include <cstdint>
#include <memory>

namespace base {
  class DictionaryValue;
}

namespace recordreplay {

bool IsRecordingOrReplaying(const char* feature = nullptr);
bool IsRecording();
bool IsReplaying();
char* GetRecordingId();

void Print(const char* format, ...);
void Diagnostic(const char* format, ...);
void Assert(const char* format, ...);
void AssertBytes(const char* why, const void* buf, size_t size);

uintptr_t RecordReplayValue(const char* why, uintptr_t v);
void RecordReplayBytes(const char* why, void* buf, size_t size);

int CreateOrderedLock(const char* name);
void OrderedLock(int lock);
void OrderedUnlock(int lock);

struct AutoOrderedLock {
  AutoOrderedLock(int id) : id_(id) { OrderedLock(id_); }
  ~AutoOrderedLock() { OrderedUnlock(id_); }
  int id_;
};

// Drop in replacement for base::Lock that can be used with
// mixed 'auto' and 'try' lockers.
class LOCKABLE ReplayBaseLock {
public:

  ReplayBaseLock(const char* ordered_name = nullptr);
  ~ReplayBaseLock();

  void Acquire() EXCLUSIVE_LOCK_FUNCTION();

  void Release() UNLOCK_FUNCTION();

  bool Try() EXCLUSIVE_TRYLOCK_FUNCTION(true);

private:
  base::Lock lock_;
  int ordered_id_;
  const char* ordered_name_;
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
bool AreEventsPassedThrough();

void BeginDisallowEvents();
void BeginDisallowEventsWithLabel(const char* label);

void EndDisallowEvents();
bool AreEventsDisallowed();

bool FeatureEnabled(const char* feature);

void BrowserEvent(const char* msg, const base::DictionaryValue& info);

struct AutoPassThroughEvents {
  AutoPassThroughEvents() { BeginPassThroughEvents(); }
  ~AutoPassThroughEvents() { EndPassThroughEvents(); }
};

struct AutoDisallowEvents {
  AutoDisallowEvents(const char* label) { BeginDisallowEventsWithLabel(label); }
  ~AutoDisallowEvents() { EndDisallowEvents(); }
};

bool HasDivergedFromRecording();
bool AllowSideEffects();

void RegisterPointer(const char* name, const void* ptr);
void UnregisterPointer(const void* ptr);
int PointerId(const void* ptr);
void* IdPointer(int id);

void OnEvent(const char* aEvent, bool aBefore);
void OnMouseEvent(const char* kind, size_t clientX, size_t clientY);
void OnKeyEvent(const char* kind, const char* key);
void OnNavigationEvent(const char* kind, const char* url);

// Create new identifiers, as with RegisterPointer/PointerId but with less
// overhead and requiring manual storage.
int NewIdMainThread(const char* name);
int NewIdAnyThread(const char* name);



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
struct ReplayPtrHash {
  static unsigned GetHash(T* key) {
    if (!recordreplay::IsRecordingOrReplaying("pointer-ids")) {
      return HashInt((uint64_t)key);
    }

    int ptr = recordreplay::PointerId(key);
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
struct ReplayRefPtrHash : ReplayPtrHash<T> {
  using ReplayPtrHash<T>::GetHash;
  static unsigned GetHash(const scoped_refptr<T>& key) {
    if (!recordreplay::IsRecordingOrReplaying("pointer-ids")) {
      return HashInt((uint64_t)key.get());
    }

    int ptr = recordreplay::PointerId(key.get());
    CHECK(ptr != 0);
    return HashInt((uint32_t)ptr);
  }
  using ReplayPtrHash<T>::Equal;
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



class SCOPED_LOCKABLE ReplayAutoLock {
public:
  explicit ReplayAutoLock(ReplayBaseLock& lock) EXCLUSIVE_LOCK_FUNCTION(lock);

  ReplayAutoLock(const ReplayAutoLock&) = delete;
  ReplayAutoLock& operator=(const ReplayAutoLock&) = delete;

  ~ReplayAutoLock() UNLOCK_FUNCTION();

private:
  ReplayBaseLock& lock_;

};



class SCOPED_LOCKABLE ReplayAutoTryLock {
 public:
  explicit ReplayAutoTryLock(ReplayBaseLock& lock) EXCLUSIVE_LOCK_FUNCTION(lock);

  ReplayAutoTryLock(const ReplayAutoTryLock&) = delete;
  ReplayAutoTryLock& operator=(const ReplayAutoTryLock&) = delete;

  ~ReplayAutoTryLock() UNLOCK_FUNCTION();

  bool is_acquired() const;

private:
  ReplayBaseLock& lock_;
  bool is_acquired_;
};



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
    if (AreEventsDisallowed()) {
      // Leak the allocated memory before destructing `unique_ptr`
      // when inside a non-deterministic execution path.
      p.release();
    }
  }
};

} // namespace recordreplay

#endif // BASE_RECORD_REPLAY_H_
