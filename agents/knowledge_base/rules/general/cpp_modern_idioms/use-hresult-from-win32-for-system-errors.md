---
name: general/use-hresult-from-win32-for-system-errors
rubric: readability
severity: should_fix
---

**Rule.** Use the `HRESULT_FROM_WIN32()` macro rather than a direct cast
(`static_cast<HRESULT>` or C-style cast) when converting Win32 system error
codes (such as `::GetLastError()` or `ERROR_*` constants) to `HRESULT`.
Conversely, do not use `HRESULT_FROM_WIN32()` on values or exception codes that
are already formatted as `HRESULT`s (such as delay-load exception codes from
`::GetExceptionCode()`); use `static_cast<HRESULT>` instead and add an
explanatory comment clarifying why the direct cast is safe (e.g., that the
enclosing `__except` filter verifies `FACILITY_VISUALCPP`).

**Why.** Win32 system error codes are unsigned non-negative integers requiring
`HRESULT_FROM_WIN32()` to set the error severity and facility bits so
`FAILED(hr)` succeeds. Wrapping values that already represent `HRESULT`s in
`HRESULT_FROM_WIN32()` corrupts their facility code and severity, while
documenting why a direct `static_cast<HRESULT>` is safe prevents misguided
suggestions to use `HRESULT_FROM_WIN32()`.

## Examples

Bad:

```cpp
// Converting Win32 error code using direct cast:
HRESULT hr = static_cast<HRESULT>(::GetLastError());

// Wrapping an existing HRESULT exception code in HRESULT_FROM_WIN32:
hr = HRESULT_FROM_WIN32(::GetExceptionCode());

// Direct cast of exception code without documenting filter safety:
hr = static_cast<HRESULT>(::GetExceptionCode());
```

Good:

```cpp
// Converting Win32 error code:
HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());

// Converting exception code known to be an HRESULT:
// The __except filter above verified FACILITY_VISUALCPP, so the code is an
// HRESULT and direct cast is safe.
HRESULT hr = static_cast<HRESULT>(::GetExceptionCode());
```
