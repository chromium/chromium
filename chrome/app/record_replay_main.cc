// Copyright 2021 Record Replay Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Routines for setting up the recorder handle when recording/replaying.
// This is directly included from files which need it to avoid linker issues.

#include "../../base/record_replay_driver.cc"

#if BUILDFLAG(IS_WIN)

#include <fcntl.h>
#include <io.h>

#else // !BUILDFLAG(IS_WIN)

#include <dlfcn.h>

#if BUILDFLAG(IS_MAC)
#include <spawn.h>
#endif

#endif // !BUILDFLAG(IS_WIN)

static void ReportFailure(const char* format, ...) {
  {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
  }
#if BUILDFLAG(IS_WIN)
  // Additionally write the message to a new file. Capturing the output written to
  // stderr by browser subprocesses on windows is surprisingly difficult.
  FILE* f = fopen("record_replay_load_recorder_error.txt", "w");
  if (f) {
    va_list args;
    va_start(args, format);
    vfprintf(f, format, args);
    fprintf(f, "\n");
    va_end(args);
    fclose(f);
  }
#endif
}

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
#if !BUILDFLAG(IS_WIN)
  void* sym = dlsym(handle, name);
#else
  void* sym = (void*)GetProcAddress((HMODULE)handle, name);
#endif
  if (!sym) {
    ReportFailure("Could not find %s in Record Replay driver.", name);
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
    ReportFailure("DoLoadDriverHandle: dlopen failed %s: %s", aPath, error ? error : "<no error>");
  }
  return handle;
#else
  HMODULE handle = LoadLibraryA(aPath);
  if (!handle && aPrintError) {
    ReportFailure("DoLoadDriverHandle: LoadLibraryA failed %s: %lu", aPath, GetLastError());
  }
  return handle;
#endif
}

#if BUILDFLAG(IS_WIN)

// On windows the driver DLL *must* have this name, as it will be used to
// lookup symbols in the driver and call them directly in various places
// that can be compiled in executables that don't contain the V8 wrappers.
static const char* WindowsDriverDLL = "windows-recordreplay.dll";

#endif // BUILDFLAG(IS_WIN)

static DriverHandle OpenDriverHandle() {
  const char* tmpdir = GetTempDirectory();
  if (!tmpdir) {
    ReportFailure("Can't figure out temporary directory, can't create driver.");
    return nullptr;
  }

  const char* driver = getenv("RECORD_REPLAY_DRIVER");
  if (driver) {
#if BUILDFLAG(IS_WIN)
    // On windows we still need to copy the driver in case it has the wrong name.
    // Don't bother checking to see if it already has the right name, this
    // setting is normally only used during internal testing.
    char driverDir[1024];
    snprintf(driverDir, sizeof(driverDir), "%s\\recordreplay-XXXXXX", tmpdir);
    _mktemp(driverDir);
    if (!CreateDirectoryA(driverDir, nullptr)) {
      ReportFailure("Creating directory for existing driver failed, can't create driver.");
      return nullptr;
    }

    char newDriver[1024];
    snprintf(newDriver, sizeof(newDriver), "%s\\%s", driverDir, WindowsDriverDLL);

    if (!CopyFileA(driver, newDriver, /* bFailIfExists */ true)) {
      ReportFailure("Copying existing driver failed, can't create driver.");
      return nullptr;
    }
    return DoLoadDriverHandle(newDriver);
#else
    return DoLoadDriverHandle(driver);
#endif
  }

  char filename[1024];
#if BUILDFLAG(IS_WIN)
  char driverDir[1024];
  snprintf(driverDir, sizeof(driverDir), "%s\\%s",
           tmpdir, recordreplay::gBuildId);
  if (!CreateDirectoryA(driverDir, nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) {
    ReportFailure("Creating directory for driver failed, can't create driver.");
    return nullptr;
  }
  snprintf(filename, sizeof(filename), "%s\\%s",
           driverDir, WindowsDriverDLL);
#else
  snprintf(filename, sizeof(filename), "%s/%s.so",
           tmpdir, recordreplay::gBuildId);
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
    ReportFailure("mkstemp failed, can't create driver.");
    return nullptr;
  }

  int nbytes = write(fd, recordreplay::gRecordReplayDriver, recordreplay::gRecordReplayDriverSize);
  if (nbytes != recordreplay::gRecordReplayDriverSize) {
    ReportFailure("write to driver temporary file failed, can't create driver.");
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
    ReportFailure("Recorder initialization warning: posix_spawn failed %d");
  }

  rv = waitpid(pid, nullptr, 0);
  if (rv < 0) {
    ReportFailure("Recorder initialization warning: waitpid failed %d", errno);
  }
#endif // BUILDFLAG(IS_MAC)

  rv = rename(tmpFilename, filename);
  if (rv < 0) {
    ReportFailure("renaming temporary driver failed");
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

// Return whether the current process should be recorded. May update the arguments.
static bool RecordReplayShouldRecord(int* pargc, const char*** pargv) {
#if BUILDFLAG(IS_WIN)

  // On windows the command line is managed through the CommandLine interface.
  base::CommandLine::Init(0, nullptr);

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  std::string type = command_line->GetSwitchValueASCII("type");

  if (type.length()) {
    // Only renderer processes are recorded/replayed.
    return type == "renderer";
  }

  // Append required switches, see below.
  command_line->AppendSwitch("no-sandbox");
  command_line->AppendSwitch("disable-gpu");

#else // !BUILDFLAG(IS_WIN)

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
    return !strcmp(type, "renderer");
  }

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

#endif // !BUILDFLAG(IS_WIN)

  return false;
}

static __attribute__((noinline)) void BusyWait() {
  fprintf(stderr, "Busy-waiting...\n");
  volatile int x = 1;
  while (x) {}
}

static void* RecordReplayAttach(int* pargc, const char*** pargv) {
  // When RECORD_REPLAY_DONT_RECORD we don't record.
  if (getenv("RECORD_REPLAY_DONT_RECORD")) {
    return nullptr;
  }

  if (!RecordReplayShouldRecord(pargc, pargv)) {
    return nullptr;
  }

  if (getenv("RECORDING_WAIT_AT_ATTACH"))
    BusyWait();

  std::string apiKey;
  const char* val = getenv("RECORD_REPLAY_API_KEY");
  if (val) {
    apiKey = val;
    // Unsetting the env var will make the variable unavailable via
    // getenv and such, and also mutates the 'environ' global, so
    // by the time gRecordReplayAttach runs, it will have no idea that
    // this value existed and won't capture it in the recording itself,
    // which is ideal for security.
#if BUILDFLAG(IS_WIN)
    _putenv("RECORD_REPLAY_API_KEY=");
#else
    unsetenv("RECORD_REPLAY_API_KEY");
#endif
  }

  void* handle = OpenDriverHandle();
  if (!handle) {
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
  gRecordReplaySaveRecording(nullptr);

#if BUILDFLAG(IS_WIN)
  // On windows we don't need to explicitly record/replay the arguments because
  // they're fetched via a recorded call, but we do need to record the executable
  // path as the recorder uses this to determine the install directory.
  char filename[1024];
  GetModuleFileNameA(nullptr, filename, sizeof(filename));
  int fake_argc = 1;
  char* fake_argv[] = { filename };
  char** pfake_argv[] = { fake_argv };
  gRecordReplayRecordCommandLineArguments(&fake_argc, pfake_argv);
#else
  gRecordReplayRecordCommandLineArguments(pargc, (char***)pargv);
#endif

  MaybeStartProfiling();

  return handle;
}
