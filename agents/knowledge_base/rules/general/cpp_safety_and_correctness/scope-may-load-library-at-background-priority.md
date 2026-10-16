---
name: general/scope-may-load-library-at-background-priority
rubric: safety
severity: should_fix
---

**Rule.** When loading DLLs explicitly (e.g. LoadLibrary, ScopedNativeLibrary)
or implicitly via Windows system APIs (e.g. SetupDiGetClassDevs) on background
or BEST_EFFORT threads, annotate the scope with
SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY() (or
SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY_REPEATEDLY()). Place the macro
directly inside the helper function performing the operation, scoped tightly
around the specific library load or system API call, rather than wrapping
external caller functions or unrelated preceding operations.

**Why.** On Windows, loading a DLL acquires the OS loader lock, causing severe
priority inversions and UI hangs if higher-priority threads block waiting for
the lock while a background thread holds it.
SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY() temporarily elevates thread
priority during the load, and encapsulating it tightly inside the helper ensures
all callers are protected without unnecessarily elevating unrelated
computations.

## Examples

Bad:

```cpp
// Bad: Calling DLL-loading Windows APIs on background threads without priority elevation,
// or placing the macro at the caller site around unrelated operations.
SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();
result.cpu = IsWin11SupportedProcessor(...);
result.memory = SysInfo::AmountOfPhysicalMemory() >= kMinTotalPhysicalMemory;
result.tpm = IsTPM20Supported();

void SampleBatteryInfo() {
  base::ScopedBlockingCall blocking_call(FROM_HERE, base::BlockingType::MAY_BLOCK);
  // Missing macro for API that triggers background DLL loading:
  base::win::ScopedDevInfo devices(::SetupDiGetClassDevs(
      &GUID_DEVICE_BATTERY, 0, 0, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE));
}
```

Good:

```cpp
// Good: Encapsulate the priority boost inside the helper, scoped tightly to the load.
result.cpu = IsWin11SupportedProcessor(...);
result.memory = SysInfo::AmountOfPhysicalMemory() >= kMinTotalPhysicalMemory;
result.tpm = IsTPM20Supported();

bool IsTPM20Supported() {
  SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();
  ScopedNativeLibrary tbs_library(LoadSystemLibrary(L"tbs.dll"));
  return tbs_library.is_valid();
}

void SampleBatteryInfo() {
  base::ScopedBlockingCall blocking_call(FROM_HERE, base::BlockingType::MAY_BLOCK);
  SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();
  base::win::ScopedDevInfo devices(::SetupDiGetClassDevs(
      &GUID_DEVICE_BATTERY, 0, 0, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE));
}
```
