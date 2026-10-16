---
name: general/distinguish-nul-character-from-null-pointer-in-comments
rubric: readability
severity: nit
---

**Rule.** In comments and documentation, refer to the '\\0' character that
terminates C-strings or character buffers as 'NUL' (e.g., 'NUL-terminated' or
'terminating NUL'), reserving 'null' for null pointers (nullptr) or empty/null
states.

**Why.** Using 'null' to describe character terminators creates ambiguity with
null pointers (nullptr), especially when documenting APIs and data structures
(such as environ or argv spans) that handle both pointer arrays and C-style
strings. Distinguishing a null pointer from a NUL-terminated character sequence
clarifies API contracts.

## Examples

Bad:

```cpp
// Returns a span over the current process' environment. Each string in the
// span is null-terminated.
BASE_EXPORT base::span<const char* const> GetEnvironment();
```

Good:

```cpp
// Returns a span over the current process' environment (excluding the
// terminating null pointer). Each string in the span is NUL-terminated.
BASE_EXPORT base::span<const char* const> GetEnvironment();
```
