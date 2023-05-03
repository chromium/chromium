// For use in situations when the recordreplay:: namespace isn't available
// due to build dependency ordering.

#if !BUILDFLAG(IS_WIN)
#include <dlfcn.h>
#else
#include <windows.h>
#endif

static void* LookupRecordReplaySymbol(const char* name) {
#if !BUILDFLAG(IS_WIN)
  void* fnptr = dlsym(RTLD_DEFAULT, name);
#else
  HMODULE module = GetModuleHandleA("windows-recordreplay.dll");
  void* fnptr = module ? (void*)GetProcAddress(module, name) : nullptr;
#endif
  return fnptr ? fnptr : reinterpret_cast<void*>(1);
}

struct RecordReplayAutoPassThroughEvents {
  void *fnBegin;
  void *fnEnd;
  bool didBegin;

  RecordReplayAutoPassThroughEvents() {
    // Cache our functions, since begin passthrough events will not allow us to
    // invoke our own op_dlsym function which can actually find our symbols.
    fnBegin = LookupRecordReplaySymbol("RecordReplayBeginPassThroughEvents");
    fnEnd = LookupRecordReplaySymbol("RecordReplayEndPassThroughEvents");

    if (fnBegin != reinterpret_cast<void*>(1)) {
      reinterpret_cast<void(*)()>(fnBegin)();
    }
  }

  ~RecordReplayAutoPassThroughEvents() {
    if (fnEnd != reinterpret_cast<void*>(1)) {
      reinterpret_cast<void(*)()>(fnEnd)();
    }
  }
};
