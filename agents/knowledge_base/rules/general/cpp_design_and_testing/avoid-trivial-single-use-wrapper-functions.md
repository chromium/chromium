---
name: general/avoid-trivial-single-use-wrapper-functions
rubric: design
severity: should_fix
---

**Rule.** Avoid introducing or retaining trivial single-use wrapper functions or
redundant internal/anonymous-namespace helpers when an underlying API, type
conversion, or public class method can be used directly. When exposing a public
method for functionality previously handled by an internal helper, inline the
implementation directly into the public method and update callers in the file to
invoke the public method rather than delegating through an intermediate
forwarding wrapper.

**Why.** Trivial forwarding wrappers and redundant internal helpers introduce
unnecessary indirection, cognitive overhead, and maintenance boilerplate without
adding meaningful abstraction. Inlining the implementation directly into the
public method or call site keeps the code concise, self-contained, and easier to
follow.

## Examples

Bad:

```cpp
// Variant 1: Trivial single-use forwarding helper around an API.
internal::WinHeapInfo WinHeapInfoFromCRTHeap() {
  return WinHeapInfoFromHandle(reinterpret_cast<HANDLE>(_get_heap_handle()));
}
auto info = WinHeapInfoFromCRTHeap();

// Variant 2: Forwarding public method to internal anonymous helper.
namespace {
std::string HardwareManufacturerName() {
  return GetSystemProperty("ro.product.manufacturer");
}
}  // namespace

std::string SysInfo::HardwareManufacturer() {
  return HardwareManufacturerName();
}
```

Good:

```cpp
// Variant 1: Call underlying API directly at the call site.
auto info = WinHeapInfoFromHandle(reinterpret_cast<HANDLE>(_get_heap_handle()));

// Variant 2: Move logic directly into the public method and update callers.
std::string SysInfo::HardwareManufacturer() {
  return GetSystemProperty("ro.product.manufacturer");
}

SysInfo::HardwareInfo SysInfo::GetHardwareInfoSync() {
  HardwareInfo info;
  info.manufacturer = HardwareManufacturer();
  return info;
}
```
