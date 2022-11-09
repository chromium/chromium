// Copyright (c) 2021 Record Replay Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/record_replay.h"
#include "base/json/json_writer.h"
#include "base/values.h"

#include <stdarg.h>

namespace recordreplay {

// Watch out for build environments where we aren't linked to V8.
#ifndef NACL_TC_REV
#define OP(RR) RR
#define OP2(RR, NORR) RR
#else
#define OP(RR)
#define OP2(RR, NORR) NORR
#endif

extern "C" bool V8IsRecordingOrReplaying(const char* feature);
extern "C" bool V8IsRecording();
extern "C" bool V8IsReplaying();
extern "C" char* V8GetRecordingId();
extern "C" void V8RecordReplayAssertVA(const char* format, va_list args);
extern "C" void V8RecordReplayAssertBytes(const char* why, const void* buf, size_t size);
extern "C" void V8RecordReplayPrintVA(const char* format, va_list args);
extern "C" void V8RecordReplayDiagnosticVA(const char* format, va_list args);
extern "C" uintptr_t V8RecordReplayValue(const char* why, uintptr_t value);
extern "C" void V8RecordReplayBytes(const char* why, void* buf, size_t size);
extern "C" size_t V8RecordReplayCreateOrderedLock(const char* name);
extern "C" void V8RecordReplayOrderedLock(int lock);
extern "C" void V8RecordReplayOrderedUnlock(int lock);
extern "C" void V8RecordReplayNewCheckpoint();
extern "C" uint64_t V8RecordReplayNewBookmark();
extern "C" void V8RecordReplayOnAnnotation(const char* kind, const char* contents);
extern "C" void V8RecordReplayOnNetworkRequest(const char* id, const char* kind, uint64_t bookmark);
extern "C" void V8RecordReplayOnNetworkRequestEvent(const char* id);
extern "C" void V8RecordReplayOnNetworkStreamStart(const char* id, const char* kind, const char* parentId);
extern "C" void V8RecordReplayOnNetworkStreamData(const char* id, size_t offset, size_t length, uint64_t bookmark);
extern "C" void V8RecordReplayOnNetworkStreamEnd(const char* id, size_t length);
extern "C" bool V8RecordReplayAreEventsDisallowed();
extern "C" void V8RecordReplayBeginDisallowEvents();
extern "C" void V8RecordReplayEndDisallowEvents();
extern "C" void V8RecordReplayBeginPassThroughEvents();
extern "C" void V8RecordReplayEndPassThroughEvents();
extern "C" bool V8RecordReplayHasDivergedFromRecording();
extern "C" void V8RecordReplayRegisterPointer(const char* name, const void* ptr);
extern "C" void V8RecordReplayUnregisterPointer(const void* ptr);
extern "C" int V8RecordReplayPointerId(const void* ptr);
extern "C" void* V8RecordReplayIdPointer(int id);
extern "C" bool V8RecordReplayFeatureEnabled(const char* feature);
extern "C" void V8RecordReplayBrowserEvent(const char* name, const char* payload);
extern "C" bool V8IsMainThread();

bool IsRecordingOrReplaying(const char* feature) {
  return OP2(V8IsRecordingOrReplaying(feature), false);
}

bool IsRecording() {
  return OP2(V8IsRecording(), false);
}

bool IsReplaying() {
  return OP2(V8IsReplaying(), false);
}

char* GetRecordingId() {
  return OP2(V8GetRecordingId(), nullptr);
}

void Assert(const char* format, ...) {
#ifndef NACL_TC_REV
  va_list ap;
  va_start(ap, format);
  V8RecordReplayAssertVA(format, ap);
  va_end(ap);
#endif
}

void Diagnostic(const char* format, ...) {
#ifndef NACL_TC_REV
  va_list ap;
  va_start(ap, format);
  V8RecordReplayDiagnosticVA(format, ap);
  va_end(ap);
#endif
}

void AssertBytes(const char* why, const void* buf, size_t size) {
  V8RecordReplayAssertBytes(why, buf, size);
}

void Print(const char* format, ...) {
#ifndef NACL_TC_REV
  va_list ap;
  va_start(ap, format);
  V8RecordReplayPrintVA(format, ap);
  va_end(ap);
#endif
}

uintptr_t RecordReplayValue(const char* why, uintptr_t v) {
  return OP2(V8RecordReplayValue(why, v), v);
}

void RecordReplayBytes(const char* why, void* buf, size_t size) {
  OP(V8RecordReplayBytes(why, buf, size));
}

size_t CreateOrderedLock(const char* name) {
  return OP2(V8RecordReplayCreateOrderedLock(name), 0);
}

void OrderedLock(int lock) {
  OP(V8RecordReplayOrderedLock(lock));
}

void OrderedUnlock(int lock) {
  OP(V8RecordReplayOrderedUnlock(lock));
}

void NewCheckpoint() {
  OP(V8RecordReplayNewCheckpoint());
}

uint64_t NewBookmark() {
  return OP(V8RecordReplayNewBookmark());
}

void OnAnnotation(const char* kind, const char* contents) {
  OP(V8RecordReplayOnAnnotation(kind, contents));
}

void OnNetworkRequest(const char* id, const char* kind, uint64_t bookmark) {
  OP(V8RecordReplayOnNetworkRequest(id, kind, bookmark));
}

void OnNetworkRequestEvent(const char* id) {
  OP(V8RecordReplayOnNetworkRequestEvent(id));
}

void OnNetworkStreamStart(const char* id, const char* kind, const char* parentId) {
  OP(V8RecordReplayOnNetworkStreamStart(id, kind, parentId));
}

void OnNetworkStreamData(const char* id, size_t offset, size_t length, uint64_t bookmark) {
  OP(V8RecordReplayOnNetworkStreamData(id, offset, length, bookmark));
}

void OnNetworkStreamEnd(const char* id, size_t length) {
  OP(V8RecordReplayOnNetworkStreamEnd(id, length));
}

bool AreEventsDisallowed() {
  return OP2(V8RecordReplayAreEventsDisallowed(), false);
}

void BeginDisallowEvents() {
  OP(V8RecordReplayBeginDisallowEvents());
}

void EndDisallowEvents() {
  OP(V8RecordReplayEndDisallowEvents());
}

void BeginPassThroughEvents() {
  OP(V8RecordReplayBeginPassThroughEvents());
}

void EndPassThroughEvents() {
  OP(V8RecordReplayEndPassThroughEvents());
}

bool FeatureEnabled(const char* feature) {
  return OP2(V8RecordReplayFeatureEnabled(feature), false);
}

void BrowserEvent(const char* name, const base::DictionaryValue& info) {
  OP({
    std::string json;
    base::JSONWriter::Write(info, &json);
    V8RecordReplayBrowserEvent(name, json.c_str());
  });
}

bool HasDivergedFromRecording() {
  return OP2(V8RecordReplayHasDivergedFromRecording(), false);
}

void RegisterPointer(const char* name, const void* ptr) {
  OP(V8RecordReplayRegisterPointer(name, ptr));
}

void UnregisterPointer(const void* ptr) {
  OP(V8RecordReplayUnregisterPointer(ptr));
}

int PointerId(const void* ptr) {
  return OP2(V8RecordReplayPointerId(ptr), 0);
}

void* IdPointer(int id) {
  return OP2(V8RecordReplayIdPointer(id), nullptr);
}

AutoLockMaybeEventsDisallowed::AutoLockMaybeEventsDisallowed(base::Lock& lock) : lock_(lock) {
  if (AreEventsDisallowed()) {
    AutoPassThroughEvents pt;
    lock.Acquire();
  } else {
    lock.Acquire();
  }
}

AutoLockMaybeEventsDisallowed::~AutoLockMaybeEventsDisallowed() {
  lock_.Release();
}

AutoUnlockMaybeEventsDisallowed::AutoUnlockMaybeEventsDisallowed(base::Lock& lock) : lock_(lock) {
  lock_.Release();
}

AutoUnlockMaybeEventsDisallowed::~AutoUnlockMaybeEventsDisallowed() {
  if (AreEventsDisallowed()) {
    AutoPassThroughEvents pt;
    lock_.Acquire();
  } else {
    lock_.Acquire();
  }
}

static int gNextMainThreadId = 1;

int NewIdMainThread(const char* name) {
  if (IsRecordingOrReplaying()) {
    if (!V8IsMainThread()) {
      fprintf(stderr, "NewIdMainThread not main thread: %s\n", name);
      CHECK(V8IsMainThread());
    }
    Assert("NewId %s", name);
    return gNextMainThreadId++;
  }
  return 0;
}

static std::atomic<int> gNextAnyThreadId{1};

int NewIdAnyThread(const char* name) {
  if (IsRecordingOrReplaying()) {
    // IDs can be created when events are disallowed when gReplayScript
    // creates URL objects. This would be nice to improve.
    if (AreEventsDisallowed())
      return 0;

    Assert("NewId %s", name);
    return RecordReplayValue("NewId", gNextAnyThreadId++);
  }
  return 0;
}

void RecordReplayString(const char* why, std::string& str) {
  size_t length = RecordReplayValue(why, str.length());
  str.resize(length);
  if (length) {
    RecordReplayBytes(why, &str[0], length);
  }
}

} // namespace recordreplay
