// Copyright (c) 2021 Record Replay Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/record_replay.h"

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

extern "C" bool V8IsRecordingOrReplaying();
extern "C" bool V8IsRecording();
extern "C" bool V8IsReplaying();
extern "C" void V8RecordReplayAssertVA(const char* format, va_list args);
extern "C" void V8RecordReplayAssertBytes(const char* why, const void* buf, size_t size);
extern "C" void V8RecordReplayPrintVA(const char* format, va_list args);
extern "C" uintptr_t V8RecordReplayValue(const char* why, uintptr_t value);
extern "C" void V8RecordReplayBytes(const char* why, void* buf, size_t size);
extern "C" size_t V8RecordReplayCreateOrderedLock(const char* name);
extern "C" void V8RecordReplayOrderedLock(int lock);
extern "C" void V8RecordReplayOrderedUnlock(int lock);
extern "C" void V8RecordReplayNewCheckpoint();
extern "C" void V8RecordReplayBeginPassThroughEvents();
extern "C" void V8RecordReplayEndPassThroughEvents();
extern "C" bool V8RecordReplayHasDivergedFromRecording();
extern "C" void V8RecordReplayRegisterPointer(void* ptr);
extern "C" void V8RecordReplayUnregisterPointer(void* ptr);
extern "C" int V8RecordReplayPointerId(void* ptr);
extern "C" void* V8RecordReplayIdPointer(int id);

bool IsRecordingOrReplaying() {
  return OP2(V8IsRecordingOrReplaying(), false);
}

bool IsRecording() {
  return OP2(V8IsRecording(), false);
}

bool IsReplaying() {
  return OP2(V8IsReplaying(), false);
}

void Assert(const char* format, ...) {
#ifndef NACL_TC_REV
  va_list ap;
  va_start(ap, format);
  V8RecordReplayAssertVA(format, ap);
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

void BeginPassThroughEvents() {
  OP(V8RecordReplayBeginPassThroughEvents());
}

void EndPassThroughEvents() {
  OP(V8RecordReplayEndPassThroughEvents());
}

bool HasDivergedFromRecording() {
  return OP2(V8RecordReplayHasDivergedFromRecording(), false);
}

void RegisterPointer(void* ptr) {
  OP(V8RecordReplayRegisterPointer(ptr));
}

void UnregisterPointer(void* ptr) {
  OP(V8RecordReplayUnregisterPointer(ptr));
}

int PointerId(void* ptr) {
  return OP2(V8RecordReplayPointerId(ptr), 0);
}

void* IdPointer(int id) {
  return OP2(V8RecordReplayIdPointer(id), nullptr);
}

} // namespace recordreplay
