---
name: general/check-empty-before-span-data-when-null-expected
rubric: correctness
severity: should_fix
---

**Rule.** Do not assume base::span::data() returns nullptr when empty. When
passing a span's data pointer to a C API, global pointer (such as environ), or
struct field that expects nullptr for an empty collection, explicitly check
span.empty() ? nullptr : span.data().

**Why.** An empty base::span or subspan can have a non-null data() pointer
(e.g., when constructed from a non-null pointer with size 0). Passing
span.data() unconditionally when empty can lead to dereferencing invalid memory
or failing callee null checks.

## Examples

Bad:

```cpp
void SetEnvironment(base::span<char*> env) {
  CHECK(env.empty() || env.back() == nullptr);
  environ = env.data();
}
```

Good:

```cpp
void SetEnvironment(base::span<char*> env) {
  CHECK(env.empty() || env.back() == nullptr);
  environ = env.empty() ? nullptr : env.data();
}
```
