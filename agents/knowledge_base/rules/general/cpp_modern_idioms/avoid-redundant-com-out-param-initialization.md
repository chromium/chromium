---
name: general/avoid-redundant-com-out-param-initialization
rubric: readability
severity: nit
---

**Rule.** Do not redundantly zero-initialize (e.g. `= 0`, `= nullptr`,
`= FALSE`) local variables that are immediately passed as `[out]` parameters to
COM or WinRT methods. By COM conventions, callees are required to initialize
output parameters upon success and clear them upon failure, and callers only
inspect them when the call succeeds.

**Why.** Standard COM interface contracts require methods to overwrite all
`[out]` parameters or set them to default/null states on failure. Because
callers must verify `SUCCEEDED(hr)` before using output values, pre-initializing
local out-parameter variables adds redundant assignments and clutter without
improving safety.

## Examples

Bad:

```cpp
UINT32 length = 0;
HRESULT hr = buffer->get_Length(&length);
if (FAILED(hr)) {
  return hr;
}

uint8_t* data = nullptr;
hr = buffer_byte_access->Buffer(&data);
if (FAILED(hr)) {
  return hr;
}
```

Good:

```cpp
UINT32 length;
HRESULT hr = buffer->get_Length(&length);
if (FAILED(hr)) {
  return hr;
}

uint8_t* data;
hr = buffer_byte_access->Buffer(&data);
if (FAILED(hr)) {
  return hr;
}
```
