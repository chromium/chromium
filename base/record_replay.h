// Copyright (c) 2021 Record Replay Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// API for interacting with the record/replay driver.

#ifndef BASE_RECORD_REPLAY_H_
#define BASE_RECORD_REPLAY_H_

#include "base/check.h"

#include <cstdint>

namespace recordreplay {

bool IsRecordingOrReplaying();
bool IsRecording();
bool IsReplaying();

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

bool AreEventsDisallowed();
void BeginPassThroughEvents();
void EndPassThroughEvents();
void BeginDisallowEvents();
void EndDisallowEvents();

struct AutoPassThroughEvents {
  AutoPassThroughEvents() { BeginPassThroughEvents(); }
  ~AutoPassThroughEvents() { EndPassThroughEvents(); }
};

struct AutoDisallowEvents {
  AutoDisallowEvents() { BeginDisallowEvents(); }
  ~AutoDisallowEvents() { EndDisallowEvents(); }
};

bool HasDivergedFromRecording();

void RegisterPointer(void* ptr);
void UnregisterPointer(void* ptr);
int PointerId(void* ptr);
void* IdPointer(int id);

// stl comparator that uses pointer IDs to compare elements when recording/replaying,
// giving a deterministic sort order.
struct CompareByPointerId {
  template <typename T>
  bool operator()(const T* a, const T* b) const {
    if (recordreplay::IsRecordingOrReplaying()) {
      int ida = recordreplay::PointerId((void*)a);
      int idb = recordreplay::PointerId((void*)b);
      CHECK(ida && idb);
      return ida < idb;
    }
    return (uintptr_t)a < (uintptr_t)b;
  }
};

} // namespace recordreplay

#endif // BASE_RECORD_REPLAY_H_
