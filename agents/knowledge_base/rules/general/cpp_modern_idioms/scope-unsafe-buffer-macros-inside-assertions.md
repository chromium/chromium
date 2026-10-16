---
name: general/scope-unsafe-buffer-macros-inside-assertions
rubric: readability
severity: should_fix
---

**Rule.** Scope `UNSAFE_BUFFERS(...)` and `UNSAFE_TODO(...)` (including
subsystem variants like `PA_UNSAFE_TODO(...)` or `PA_UNSAFE_BUFFERS(...)`)
tightly to the innermost, minimal unsafe expression (such as raw pointer
arithmetic, unchecked indexing, address-of calculations, or C-string calls).
When taking the address of an unsafe element, place the address-of operator `&`
inside the macro (e.g., `UNSAFE_BUFFERS(&array[index])`) rather than outside
(`&UNSAFE_BUFFERS(...)`). Do not enclose outer assertion macros (`EXPECT_*`,
`ASSERT_*`, `CHECK`), entire loops, multi-statement compound blocks (`{ ... }`),
assignments, comparisons, or constant index-zero subscripting (`ptr[0]` or
`argv[0]`), which is equivalent to a pointer dereference and not flagged by
`-Wunsafe-buffer-usage`.

**Why.** Unsafe buffer macros expand to compiler `_Pragma` directives. Placing
unary operators like `&` outside the macro attaches operators across pragma
boundaries instead of encapsulating the full address calculation, while wrapping
outer assertion macros, assignments, comparisons, entire loops, compound blocks,
or constant index-zero dereferences unnecessarily opts surrounding safe
operations out of compiler buffer-safety diagnostics, adds boilerplate noise,
and obscures which operations require exemption.

## Examples

Bad:

```cpp
// Bad: Placing unary address-of operator outside unsafe macro
PartitionRoot::Bucket* bucket =
    &PA_UNSAFE_TODO(allocator.root()->buckets_[test_bucket_index_]);

// Bad: Wrapping outer assertion macro
UNSAFE_TODO(EXPECT_EQ(0, memcmp(dst, "abcdef", sizeof(dst[0]) * 7)));

// Bad: Wrapping entire assignment operation
PA_UNSAFE_TODO(ref[sz - 1] = '\000');

// Bad: Wrapping constant index-zero subscripting (equivalent to dereference)
fprintf(stderr, "Usage: %s <dll name>\n", UNSAFE_TODO(argv[0]));

// Bad: Wrapping entire loop and safe container/arithmetic logic
UNSAFE_BUFFERS({
  for (const wchar_t* p = env_ptr; *p; p += wcslen(p) + 1) {
    const size_t offset = static_cast<size_t>(p - env_ptr);
    size_t line_length = wcslen(p) + 1;
    block_size = offset + line_length;
    str_views.push_back(std::make_pair(offset, line_length));
  }
});
```

Good:

```cpp
// Good: Place unary address-of operator inside unsafe macro
PartitionRoot::Bucket* bucket =
    PA_UNSAFE_TODO(&allocator.root()->buckets_[test_bucket_index_]);

// Good: Wrap only innermost unsafe call inside assertion
EXPECT_EQ(0, UNSAFE_TODO(memcmp(dst, "abcdef", sizeof(dst[0]) * 7)));

// Good: Wrap only the unsafe indexing expression on LHS of assignment
PA_UNSAFE_TODO(ref[sz - 1]) = '\000';

// Good: Access index 0 directly without UNSAFE_TODO/UNSAFE_BUFFERS
fprintf(stderr, "Usage: %s <dll name>\n", argv[0]);

// Good: Wrap only minimal unsafe pointer arithmetic and C-string calls
const wchar_t* p = env_ptr;
while (*p) {
  // SAFETY: env_ptr points to OS-provided null-terminated strings.
  const size_t length = UNSAFE_BUFFERS(wcslen(p));
  const size_t offset = static_cast<size_t>(p - env_ptr);
  block_size = offset + length + 1;
  str_views.push_back(std::make_pair(offset, length));
  // SAFETY: Advancing past the verified string and terminating null.
  p = UNSAFE_BUFFERS(p + length + 1);
}
```
