---
name: general/pass-target-com-interface-directly-to-cocreateinstance
rubric: readability
severity: nit
---

**Rule.** Pass the target COM interface pointer directly to CoCreateInstance via
IID_PPV_ARGS(&target_ptr) rather than creating an intermediate IUnknown pointer
and querying the desired interface via .As() or QueryInterface().

**Why.** IID_PPV_ARGS automatically deduces the requested COM interface IID and
casts the target pointer to void\*\*. Creating an intermediate IUnknown pointer
and then querying the desired interface adds unnecessary boilerplate, extra
local variables, redundant error handling, and an unneeded query call when
IUnknown itself is not required.

## Examples

Bad:

```cpp
Microsoft::WRL::ComPtr<IUnknown> unknown;
HRESULT hr = ::CoCreateInstance(kClsid, nullptr, CLSCTX_LOCAL_SERVER,
                                IID_PPV_ARGS(&unknown));
if (FAILED(hr)) {
  return hr;
}
Microsoft::WRL::ComPtr<IElevator2> elevator;
hr = unknown.As(&elevator);
```

Good:

```cpp
Microsoft::WRL::ComPtr<IElevator2> elevator;
HRESULT hr = ::CoCreateInstance(kClsid, nullptr, CLSCTX_LOCAL_SERVER,
                                IID_PPV_ARGS(&elevator));
if (FAILED(hr)) {
  return hr;
}
```
