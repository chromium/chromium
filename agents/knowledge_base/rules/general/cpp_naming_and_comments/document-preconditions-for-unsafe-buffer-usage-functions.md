---
name: general/document-preconditions-for-unsafe-buffer-usage-functions
rubric: readability
severity: nit
---

**Rule.** When declaring a function or method annotated with
`UNSAFE_BUFFER_USAGE`, explicitly document in its declaration comment why it
requires the attribute, the specific memory safety hazards involved, and the
caller-guaranteed preconditions (such as pointer validity, buffer sizes, or
NUL-termination) required for safe invocation.

**Why.** Annotating a function with `UNSAFE_BUFFER_USAGE` forces all callers to
wrap calls in `UNSAFE_BUFFERS(...)` and justify memory safety in a `// SAFETY:`
comment. Without callee documentation specifying what hazards exist and what
caller preconditions must be guaranteed, callers and reviewers cannot verify or
formulate valid safety justifications.

## Examples

Bad:

```cpp
UNSAFE_BUFFER_USAGE BASE_EXPORT base::HeapArray<char*> AlterEnvironment(
    base::span<const char* const> env,
    const EnvironmentMap& changes);
```

Good:

```cpp
// This function is marked `UNSAFE_BUFFER_USAGE` because it relies on the
// caller-guaranteed precondition that every string in `env` is NUL-terminated.
// The function cannot verify this precondition at compile time or safely at
// runtime, and passing non-NUL-terminated strings will result in
// out-of-bounds memory reads (undefined behavior).
UNSAFE_BUFFER_USAGE BASE_EXPORT base::HeapArray<char*> AlterEnvironment(
    base::span<const char* const> env,
    const EnvironmentMap& changes);
```
