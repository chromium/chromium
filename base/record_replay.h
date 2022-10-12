// Copyright (c) 2021 Record Replay Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// API for interacting with the record/replay driver.

#ifndef BASE_RECORD_REPLAY_H_
#define BASE_RECORD_REPLAY_H_

#include "base/check.h"
#include "base/optional.h"
#include "base/synchronization/lock.h"

#include <cstdint>

namespace base {
  class DictionaryValue;
}

namespace recordreplay {

bool IsRecordingOrReplaying();
bool IsRecording();
bool IsReplaying();
char* GetRecordingId();

void Print(const char* format, ...);
void Diagnostic(const char* format, ...);
void Assert(const char* format, ...);
void AssertBytes(const char* why, const void* buf, size_t size);

uintptr_t RecordReplayValue(const char* why, uintptr_t v);
void RecordReplayBytes(const char* why, void* buf, size_t size);

size_t CreateOrderedLock(const char* name);
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
void OnNetworkRequest(const char* id, const char* kind, uint64_t bookmark);
void OnNetworkRequestEvent(const char* id);
void OnNetworkStreamStart(const char* id, const char* kind, const char* parentId);
void OnNetworkStreamData(const char* id, size_t offset, size_t length, uint64_t bookmark);
void OnNetworkStreamEnd(const char* id, size_t length);

bool AreEventsDisallowed();
void BeginPassThroughEvents();
void EndPassThroughEvents();
void BeginDisallowEvents();
void EndDisallowEvents();

bool FeatureEnabled(const char* feature);

void BrowserEvent(const char* msg, const base::DictionaryValue& info);

struct AutoPassThroughEvents {
  AutoPassThroughEvents() { BeginPassThroughEvents(); }
  ~AutoPassThroughEvents() { EndPassThroughEvents(); }
};

struct AutoDisallowEvents {
  AutoDisallowEvents() { BeginDisallowEvents(); }
  ~AutoDisallowEvents() { EndDisallowEvents(); }
};

bool HasDivergedFromRecording();

void RegisterPointer(const void* ptr);
void UnregisterPointer(const void* ptr);
int PointerId(const void* ptr);
void* IdPointer(int id);

// stl comparator that uses pointer IDs to compare elements when recording/replaying,
// giving a deterministic sort order.
struct CompareByPointerId {
  template <typename T>
  bool operator()(const T* a, const T* b) const {
    if (recordreplay::IsRecordingOrReplaying()) {
      int ida = recordreplay::PointerId(a);
      int idb = recordreplay::PointerId(b);
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
    if (IsRecordingOrReplaying()) {
      int ida = PointerId(a.get());
      int idb = PointerId(b.get());
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
    if (recordreplay::IsRecordingOrReplaying()) {
      int ida = recordreplay::PointerId(a.Get());
      int idb = recordreplay::PointerId(b.Get());
      CHECK(ida && idb);
      return ida < idb;
    }
    return a < b;
  }
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

} // namespace recordreplay

#endif // BASE_RECORD_REPLAY_H_
