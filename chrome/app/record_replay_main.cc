// Copyright 2021 Record Replay Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Routines for setting up the recorder handle when recording/replaying.
// This is directly included from files which need it to avoid linker issues.

#include "../../base/record_replay_driver.cc"

#if BUILDFLAG(IS_MAC)
#include <spawn.h>
#endif

static void (*gRecordReplayAttach)(const char* dispatchAddress, const char* buildId);
static void (*gRecordReplaySetApiKey)(const char* apiKey);
static void (*gRecordReplayProfileExecution)(const char* path);
static void (*gRecordReplayRecordCommandLineArguments)(int*, char***);
static void (*gRecordReplaySaveRecording)(const char* dir);

template <typename Src, typename Dst>
static inline void CastPointer(const Src src, Dst* dst) {
  static_assert(sizeof(Src) == sizeof(uintptr_t), "bad size");
  static_assert(sizeof(Dst) == sizeof(uintptr_t), "bad size");
  memcpy((void*)dst, (const void*)&src, sizeof(uintptr_t));
}

template <typename T>
static void RecordReplayLoadSymbol(void* handle, const char* name, T& function) {
  void* sym = dlsym(handle, name);
  if (!sym) {
    fprintf(stderr, "Could not find %s in Record Replay driver.\n", name);
    return;
  }

  CastPointer(sym, &function);
}

static const char* GetTempDirectory() {
#if !BUILDFLAG(IS_WIN)
  const char* tmpdir = getenv("TMPDIR");
  return tmpdir ? tmpdir : "/tmp";
#else
  return getenv("TEMP");
#endif
}

typedef void* DriverHandle;

static DriverHandle DoLoadDriverHandle(const char* aPath, bool aPrintError = true) {
#if !BUILDFLAG(IS_WIN)
  void* handle = dlopen(aPath, RTLD_LAZY);
  if (!handle && aPrintError) {
    char* error = dlerror();
    fprintf(stderr, "DoLoadDriverHandle: dlopen failed %s: %s\n", aPath, error ? error : "<no error>");
  }
  return handle;
#else
  HMODULE handle = LoadLibraryA(aPath);
  if (!handle && aPrintError) {
    fprintf(stderr, "DoLoadDriverHandle: LoadLibraryA failed %s: %u\n", aPath, GetLastError());
  }
  return handle;
#endif
}

static DriverHandle OpenDriverHandle() {
  const char* driver = getenv("RECORD_REPLAY_DRIVER");
  if (driver) {
    return DoLoadDriverHandle(driver);
  }

  const char* tmpdir = GetTempDirectory();
  if (!tmpdir) {
    fprintf(stderr, "Can't figure out temporary directory, can't create driver.\n");
    return nullptr;
  }

  char filename[1024];
#if !BUILDFLAG(IS_WIN)
  snprintf(filename, sizeof(filename), "%s/recordreplay-%s.so", tmpdir, recordreplay::gBuildId);
#else
  snprintf(filename, sizeof(filename), "%s\\recordreplay-%s.dll", tmpdir, recordreplay::gBuildId);
#endif

  DriverHandle handle = DoLoadDriverHandle(filename, /* aPrintError */ false);
  if (handle) {
    return handle;
  }

  char tmpFilename[1024];
#if !BUILDFLAG(IS_WIN)
  snprintf(tmpFilename, sizeof(tmpFilename), "%s/recordreplay.so-XXXXXX", tmpdir);
  int fd = mkstemp(tmpFilename);
#else
  int fd;
  for (int i = 0; i < 10; i++) {
    snprintf(tmpFilename, sizeof(tmpFilename), "%s\\recordreplay.dll-XXXXXX", tmpdir);
    _mktemp(tmpFilename);
    fd = _open(tmpFilename, O_CREAT | O_TRUNC | O_WRONLY | O_BINARY);
    if (fd >= 0) {
      break;
    }
  }
  #define write _write
  #define close _close
#endif
  if (fd < 0) {
    fprintf(stderr, "mkstemp failed, can't create driver.\n");
    return nullptr;
  }

  int nbytes = write(fd, recordreplay::gRecordReplayDriver, recordreplay::gRecordReplayDriverSize);
  if (nbytes != recordreplay::gRecordReplayDriverSize) {
    fprintf(stderr, "write to driver temporary file failed, can't create driver.\n");
    return nullptr;
  }

  close(fd);

  int rv;

#if BUILDFLAG(IS_MAC)
  // Strip any quarantine flag on the written file, if necessary, so that
  // the file can be run or loaded into a process. macOS quarantines any
  // files created by the browser even if they are related to the update
  // process.
  char* args[] = {
    strdup("/usr/bin/xattr"),
    strdup("-d"),
    strdup("com.apple.quarantine"),
    tmpFilename,
    nullptr,
  };
  char* empty_environ = nullptr;
  pid_t pid;
  rv = posix_spawn(&pid, "/usr/bin/xattr", nullptr, nullptr, args, &empty_environ);
  if (rv < 0) {
    fprintf(stderr, "Recorder initialization warning: posix_spawn failed %d\n", errno);
  }

  rv = waitpid(pid, nullptr, 0);
  if (rv < 0) {
    fprintf(stderr, "Recorder initialization warning: waitpid failed %d\n", errno);
  }
#endif // XP_MACOSX

  rv = rename(tmpFilename, filename);
  if (rv < 0) {
    fprintf(stderr, "renaming temporary driver failed\n");
  }

  return DoLoadDriverHandle(filename);
}

static void MaybeStartProfiling() {
  const char* directory = getenv("RECORD_REPLAY_PROFILE_DIRECTORY");
  if (!directory) {
    return;
  }

  char path[1000];
  snprintf(path, sizeof(path), "%s/profile-%d.log", directory, rand());

  gRecordReplayProfileExecution(path);
}

static void* RecordReplayAttach(int* pargc, const char*** pargv) {
  // Figure out what type of process this is.
  const char* type = nullptr;
  for (int i = 0; i < *pargc; i++) {
    if (!strncmp((*pargv)[i], "--type=", 7)) {
      type = (*pargv)[i] + 7;
      break;
    }
  }
  if (type) {
    // Only renderer processes are recorded/replayed.
    if (strcmp(type, "renderer")) {
      return nullptr;
    }
  } else {
    // If there is no type, this is the main process. Add a couple command line
    // arguments which are required to record/replay.
    const char** nargv = new const char*[*pargc + 3];
    memcpy(nargv, *pargv, *pargc * sizeof(char*));
    *pargv = nargv;

    // Recording processes currently need the sandbox disabled in order to
    // write out recording IDs to the specified path name.
    (*pargv)[*pargc] = strdup("--no-sandbox");

    // Recording/replaying currently requires software rendering.
    (*pargv)[*pargc + 1] = strdup("--disable-gpu");

    (*pargv)[*pargc + 2] = nullptr;
    *pargc += 2;

    return nullptr;
  }

  // When RECORD_REPLAY_DONT_RECORD we don't record, though the main browser
  // process will still be configured as if we are recording, see above.
  if (getenv("RECORD_REPLAY_DONT_RECORD")) {
    return nullptr;
  }

  std::string apiKey;
  const char* val = getenv("RECORD_REPLAY_API_KEY");
  if (val) {
    apiKey = val;
    // Unsetting the env var will make the variable unavailable via
    // getenv and such, and also mutates the 'environ' global, so
    // by the time gRecordReplayAttach runs, it will have no idea that
    // this value existed and won't capture it in the recording itself,
    // which is ideal for security.
    unsetenv("RECORD_REPLAY_API_KEY");
  }

  void* handle = OpenDriverHandle();
  if (!handle) {
    const char* error = dlerror();
    fprintf(stderr, "Loading Record Replay driver failed: %s\n",
            error ? error : "<no error>");
    return nullptr;
  }

  RecordReplayLoadSymbol(handle, "RecordReplayAttach", gRecordReplayAttach);
  RecordReplayLoadSymbol(handle, "RecordReplaySetApiKey", gRecordReplaySetApiKey);
  RecordReplayLoadSymbol(handle, "RecordReplayProfileExecution", gRecordReplayProfileExecution);
  RecordReplayLoadSymbol(handle, "RecordReplaySaveRecording", gRecordReplaySaveRecording);
  RecordReplayLoadSymbol(handle, "RecordReplayRecordCommandLineArguments",
                         gRecordReplayRecordCommandLineArguments);

  if (apiKey.length()) {
    gRecordReplaySetApiKey(apiKey.c_str());
  }

  const char* dispatchAddress = getenv("RECORD_REPLAY_SERVER");

  gRecordReplayAttach(dispatchAddress, recordreplay::gBuildId);
  gRecordReplayRecordCommandLineArguments(pargc, (char***)pargv);
  gRecordReplaySaveRecording(nullptr);

  MaybeStartProfiling();

  return handle;
}
