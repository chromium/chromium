---
name: general/match-call-site-cast-to-parameter-type
rubric: readability
severity: should_fix
---

**Rule.** When an argument requires an explicit cast at a call site, cast
directly to the callee parameter's declared type (such as
`reinterpret_cast<void*>` when the parameter is `void*`) rather than to an
intermediate alias or typedef (such as `HANDLE`), or update the callee's
parameter type to the specific domain type.

**Why.** Casting an argument to an intermediate type or typedef that differs
from the parameter's declared type creates a misleading type mismatch between
the call site and the function signature. Targeting the declared type directly
clarifies the conversion and avoids confusion about the callee's interface.

## Examples

Bad:

```cpp
// Callee declaration takes `void*`:
void WinHeapMemoryDumpImpl(void* heap, WinHeapInfo* info);

// Bad: casting to `HANDLE` (a typedef for void*) when parameter is `void*`
WinHeapMemoryDumpImpl(
    reinterpret_cast<HANDLE>(_get_heap_handle()), &info);
```

Good:

```cpp
// Callee declaration takes `void*`:
void WinHeapMemoryDumpImpl(void* heap, WinHeapInfo* info);

// Good: cast directly to the parameter's declared type (`void*`)
WinHeapMemoryDumpImpl(
    reinterpret_cast<void*>(_get_heap_handle()), &info);

// Alternatively, declare the callee parameter with the specific domain type:
void WinHeapMemoryDumpImpl(HANDLE heap, WinHeapInfo* info);
```
