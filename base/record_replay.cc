// Copyright (c) 2021 Record Replay Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/record_replay.h"
#include "base/json/json_writer.h"
#include "base/values.h"

#include <stdarg.h>

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace recordreplay {

#define ForEachV8API(Macro)                                             \
  Macro(V8IsRecordingOrReplaying,                                       \
        (const char* feature), (feature), bool, false)                  \
  Macro(V8IsRecording, (), (), bool, false)                             \
  Macro(V8IsReplaying, (), (), bool, false)                             \
  Macro(V8GetRecordingId, (), (), char*, nullptr)                       \
  Macro(V8RecordReplayValue,                                            \
        (const char* why, uintptr_t value), (why, value), uintptr_t, value) \
  Macro(V8RecordReplayCreateOrderedLock,                                \
        (const char* name), (name), size_t, 0)                          \
  Macro(V8RecordReplayNewBookmark, (), (), uint64_t, 0)                 \
  Macro(V8RecordReplayAreEventsDisallowed, (), (), bool, false)         \
  Macro(V8RecordReplayAreEventsPassedThrough, (), (), bool, false)      \
  Macro(V8RecordReplayHasDivergedFromRecording, (), (), bool, false)    \
  Macro(V8RecordReplayAllowSideEffects, (), (), bool, true)             \
  Macro(V8RecordReplayPointerId, (const void* ptr), (ptr), int, 0)      \
  Macro(V8RecordReplayIdPointer, (int id), (id), void*, nullptr)        \
  Macro(V8RecordReplayFeatureEnabled,                                   \
        (const char* feature), (feature), bool, false)                  \
  Macro(V8IsMainThread, (), (), bool, false)                            \
  Macro(V8RecordReplayHadMismatch, (), (), bool, false)

#define ForEachV8APIVoid(Macro)                                         \
  Macro(V8RecordReplayAssertVA,                                         \
        (const char* format, va_list args),                             \
        (format, args))                                                 \
  Macro(V8RecordReplayAssertBytes,                                      \
        (const char* why, const void* buf, size_t size),                \
        (why, buf, size))                                               \
  Macro(V8RecordReplayPrintVA,                                          \
        (const char* format, va_list args),                             \
        (format, args))                                                 \
  Macro(V8RecordReplayDiagnosticVA,                                     \
        (const char* format, va_list args),                             \
        (format, args))                                                 \
  Macro(V8RecordReplayWarning,                                          \
        (const char* format, va_list args),                             \
        (format, args))                                                 \
  Macro(V8RecordReplayBytes,                                            \
        (const char* why, void* buf, size_t size),                      \
        (why, buf, size))                                               \
  Macro(V8RecordReplayOrderedLock, (int lock), (lock))                  \
  Macro(V8RecordReplayOrderedUnlock, (int lock), (lock))                \
  Macro(V8RecordReplayNewCheckpoint, (), ())                            \
  Macro(V8RecordReplayOnAnnotation,                                     \
        (const char* kind, const char* contents),                       \
        (kind, contents))                                               \
  Macro(V8RecordReplayOnNetworkRequest,                                 \
    (const char* id, const char* kind, uint64_t bookmark),              \
    (id, kind, bookmark))                                               \
  Macro(V8RecordReplayOnNetworkRequestEvent, (const char* id), (id))    \
  Macro(V8RecordReplayOnNetworkStreamStart,                             \
        (const char* id, const char* kind, const char* parentId),       \
        (id, kind, parentId))                                           \
  Macro(V8RecordReplayOnNetworkStreamData,                              \
        (const char* id, size_t offset, size_t length, uint64_t bookmark), \
        (id, offset, length, bookmark))                                 \
  Macro(V8RecordReplayOnNetworkStreamEnd,                               \
        (const char* id, size_t length), (id, length))                  \
  Macro(V8RecordReplayBeginDisallowEvents, (), ())                      \
  Macro(V8RecordReplayBeginDisallowEventsWithLabel,                     \
        (const char* label), (label))                                   \
  Macro(V8RecordReplayEndDisallowEvents, (), ())                        \
  Macro(V8RecordReplayBeginPassThroughEvents, (), ())                   \
  Macro(V8RecordReplayEndPassThroughEvents, (), ())                     \
  Macro(V8RecordReplayRegisterPointer,                                  \
        (const char* name, const void* ptr), (name, ptr))               \
  Macro(V8RecordReplayUnregisterPointer, (const void* ptr), (ptr))      \
  Macro(V8RecordReplayBrowserEvent,                                     \
        (const char* name, const char* payload), (name, payload))       \
  Macro(V8RecordReplayOnEvent,                                          \
        (const char* event, bool before), (event, before))              \
  Macro(V8RecordReplayOnMouseEvent,                                     \
        (const char* kind, size_t clientX, size_t clientY),             \
        (kind, clientX, clientY))                                       \
  Macro(V8RecordReplayOnKeyEvent,                                       \
        (const char* kind, const char* key), (kind, key))               \
  Macro(V8RecordReplayOnNavigationEvent,                                \
        (const char* kind, const char* url), (kind, url))               \
  Macro(V8RecordReplayAddOrderedSRWLock,                                \
        (const char* name, void* lock), (name, lock))                   \
  Macro(V8RecordReplayRemoveOrderedSRWLock, (void* lock), (lock))       \
  Macro(V8RecordReplayMaybeTerminate,                                   \
        (void (*callback)(void*), void* data), (callback, data))        \
  Macro(V8RecordReplayGetCurrentJSStack,                                \
        (std::string* stackTrace), (stackTrace))

#if BUILDFLAG(IS_WIN)

#define DefineFunction(Name, Formals, Args, ReturnType, DefaultValue) \
  static ReturnType (*g##Name) Formals;                               \
  static inline ReturnType Name Formals {                             \
    return g##Name ? g##Name Args : DefaultValue;                     \
  }
ForEachV8API(DefineFunction)
#undef DefineFunction

#define DefineFunctionVoid(Name, Formals, Args) \
  static void (*g##Name) Formals;               \
  static inline void Name Formals {             \
    if (g##Name)                                \
      g##Name Args;                             \
  }
ForEachV8APIVoid(DefineFunctionVoid)
#undef DefineFunctionVoid

static void InitializationError(const char* format, ...) {
  {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
  }

#if BUILDFLAG(IS_WIN)
  // Additionally write the message to a new file. Capturing the output written to
  // stderr by browser subprocesses on windows is surprisingly difficult.
  const char* dir = getenv("RECORD_REPLAY_LOG_DIRECTORY");
  char file[1024];
  snprintf(file, sizeof(file), "%s\\record_replay_initialization_error.txt", dir ? dir : ".");
  FILE* f = fopen(file, "w");
  if (f) {
    va_list args;
    va_start(args, format);
    vfprintf(f, format, args);
    va_end(args);
    fclose(f);
  }
#endif

  CHECK(0);
}

void InitBindings() {
  HMODULE module;
  if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
			  reinterpret_cast<LPCSTR>(InitBindings),
			  &module)) {
    InitializationError("GetModuleHandleExA failed %d, crashing...\n", (int)GetLastError());
  }

#define LoadFunction(Name, Formals, Args, ReturnType, DefaultValue)           \
  g##Name = reinterpret_cast<ReturnType(*)Formals>(GetProcAddress(module, #Name)); \
  if (!g##Name) {                                                             \
    InitializationError("Could not find V8 export %s, crashing...\n", #Name); \
  }
ForEachV8API(LoadFunction)
#undef LoadFunction

#define LoadFunctionVoid(Name, Formals, Args)                                 \
  g##Name = reinterpret_cast<void(*)Formals>(GetProcAddress(module, #Name)); \
  if (!g##Name) {                                                             \
    InitializationError("Could not find V8 export %s, crashing...\n", #Name); \
  }
ForEachV8APIVoid(LoadFunctionVoid)
#undef LoadFunctionVoid
}

#else // !BUILD_FLAG(IS_WIN)

#define DefineFunction(Name, Formals, Args, ReturnType, DefaultValue) \
  extern "C" ReturnType Name Formals;
ForEachV8API(DefineFunction)
#undef DefineFunction

#define DefineFunctionVoid(Name, Formals, Args) \
  extern "C" void Name Formals;
ForEachV8APIVoid(DefineFunctionVoid)
#undef DefineFunction

#endif // !BUILD_FLAG(IS_WIN)

bool IsRecordingOrReplaying(const char* feature) {
  return V8IsRecordingOrReplaying(feature);
}

bool IsRecording() {
  return V8IsRecording();
}

bool IsReplaying() {
  return V8IsReplaying();
}

char* GetRecordingId() {
  return V8GetRecordingId();
}

bool HadMismatch() {
  return V8RecordReplayHadMismatch();
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

void Warning(const char* format, ...) {
#ifndef NACL_TC_REV
  va_list ap;
  va_start(ap, format);
  V8RecordReplayWarning(format, ap);
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
  return V8RecordReplayValue(why, v);
}

void RecordReplayBytes(const char* why, void* buf, size_t size) {
  V8RecordReplayBytes(why, buf, size);
}

int CreateOrderedLock(const char* name) {
  return (int)V8RecordReplayCreateOrderedLock(name);
}

void OrderedLock(int lock) {
  V8RecordReplayOrderedLock(lock);
}

void OrderedUnlock(int lock) {
  V8RecordReplayOrderedUnlock(lock);
}

void NewCheckpoint() {
  V8RecordReplayNewCheckpoint();
}

uint64_t NewBookmark() {
  return V8RecordReplayNewBookmark();
}

void OnAnnotation(const char* kind, const char* contents) {
  V8RecordReplayOnAnnotation(kind, contents);
}

void OnNetworkRequest(const char* id, const char* kind, uint64_t bookmark) {
  V8RecordReplayOnNetworkRequest(id, kind, bookmark);
}

void OnNetworkRequestEvent(const char* id) {
  V8RecordReplayOnNetworkRequestEvent(id);
}

void OnNetworkStreamStart(const char* id, const char* kind, const char* parentId) {
  V8RecordReplayOnNetworkStreamStart(id, kind, parentId);
}

void OnNetworkStreamData(const char* id, size_t offset, size_t length, uint64_t bookmark) {
  V8RecordReplayOnNetworkStreamData(id, offset, length, bookmark);
}

void OnNetworkStreamEnd(const char* id, size_t length) {
  V8RecordReplayOnNetworkStreamEnd(id, length);
}

bool AreEventsDisallowed() {
  return V8RecordReplayAreEventsDisallowed();
}

void BeginDisallowEvents() {
  V8RecordReplayBeginDisallowEvents();
}

void BeginDisallowEventsWithLabel(const char* label) {
  V8RecordReplayBeginDisallowEventsWithLabel(label);
}

void EndDisallowEvents() {
  V8RecordReplayEndDisallowEvents();
}

bool AreEventsPassedThrough() {
  return V8RecordReplayAreEventsPassedThrough();
}

void BeginPassThroughEvents() {
  V8RecordReplayBeginPassThroughEvents();
}

void EndPassThroughEvents() {
  V8RecordReplayEndPassThroughEvents();
}

bool FeatureEnabled(const char* feature) {
  return V8RecordReplayFeatureEnabled(feature);
}

void GetCurrentJSStack(std::string* stackTrace) {
  return V8RecordReplayGetCurrentJSStack(stackTrace);
}

void BrowserEvent(const char* name, const base::DictionaryValue& info) {
  std::string json;
  base::JSONWriter::Write(info, &json);
  V8RecordReplayBrowserEvent(name, json.c_str());
}

bool HasDivergedFromRecording() {
  return V8RecordReplayHasDivergedFromRecording();
}

bool AllowSideEffects() {
  return V8RecordReplayAllowSideEffects();
}

void RegisterPointer(const char* name, const void* ptr) {
  V8RecordReplayRegisterPointer(name, ptr);
}

void UnregisterPointer(const void* ptr) {
  V8RecordReplayUnregisterPointer(ptr);
}

int PointerId(const void* ptr) {
  return V8RecordReplayPointerId(ptr);
}

void* IdPointer(int id) {
  return V8RecordReplayIdPointer(id);
}

void OnEvent(const char* aEvent, bool aBefore) {
  V8RecordReplayOnEvent(aEvent, aBefore);
}
void OnMouseEvent(const char* kind,
                                size_t clientX,
                                size_t clientY) {
  V8RecordReplayOnMouseEvent(kind, clientX, clientY);
}
void OnKeyEvent(const char* kind, const char* key) {
  V8RecordReplayOnKeyEvent(kind, key);
}
void OnNavigationEvent(const char* kind, const char* url) {
  V8RecordReplayOnNavigationEvent(kind, url);
}

AutoLockMaybeEventsDisallowed::AutoLockMaybeEventsDisallowed(
        base::Lock& lock)
    : lock_(lock) {
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

bool IsMainThread() {
  return V8IsMainThread();
}

static int gNextMainThreadId = 1;

static bool CheckNewId(const char* name) {
  if (!IsRecordingOrReplaying()) {
    // Don't track anything.
    return false;
  }
  if (HasDivergedFromRecording()) {
    // Everything is allowed when explicitly diverged.
    return true;
  }
  if (AreEventsDisallowed()) {
    // IDs can be created when events are disallowed when our own scripts
    // create URL objects. This would be nice to improve.
    if (!IsInReplayCode()) {
      Warning("NewId when not allowed %s", name);
    }
    return false;
  }
  Assert("NewId %s", name);
  return true;
}

int NewIdMainThread(const char* name) {
  if (!CheckNewId(name)) {
    return 0;
  }
  if (!V8IsMainThread()) {
    fprintf(stderr, "NewIdMainThread not main thread: %s\n", name);
    CHECK(V8IsMainThread());
  }
  return gNextMainThreadId++;
}

static std::atomic<int> gNextAnyThreadId{1};

int NewIdAnyThread(const char* name) {
  if (!CheckNewId(name)) {
    return 0;
  }
  return (int)RecordReplayValue("NewId", (uintptr_t)gNextAnyThreadId++);
}

bool IsInReplayCode() {
  // Events are disallowed when running Replay's own scripts.
  // FIXME Add to Recording API.
  // https://linear.app/replay/issue/RUN-1502
  return IsReplaying() && AreEventsDisallowed();
}


void RecordReplayString(const char* why, std::string& str) {
  size_t length = RecordReplayValue(why, str.length());
  str.resize(length);
  if (length) {
    RecordReplayBytes(why, &str[0], length);
  }
}

void AddOrderedSRWLock(const char* name, void* lock) {
  V8RecordReplayAddOrderedSRWLock(name, lock);
}

void RemoveOrderedSRWLock(void* lock) {
  V8RecordReplayRemoveOrderedSRWLock(lock);
}

void MaybeTerminate(void (*callback)(void*), void* data) {
  V8RecordReplayMaybeTerminate(callback, data);
}

} // namespace recordreplay
