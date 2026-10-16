---
name: general/value-initialize-std-array-buffers
rubric: correctness
severity: nit
---

**Rule.** When declaring `std::array` buffers with fundamental, scalar, or
primitive element types (such as `uint8_t`, `char`, `int`, or pointers),
explicitly value-initialize the container using `= {}` (or `{}`). Do not leave
`std::array` default-initialized without braces when replacing zero-initialized
C-style arrays or `memset()` calls.

**Why.** Default initialization of `std::array<T, N>` does not zero-initialize
elements of fundamental or primitive types, leaving them with indeterminate
values. Replacing a zero-initialized C array or `memset()` with an uninitialized
`std::array` causes uninitialized memory reads and undefined behavior.

## Examples

Bad:

```cpp
// Default initialization leaves primitive elements uninitialized:
const size_t buffer_size = 50;
std::array<uint8_t, buffer_size> buffer;

// Dropping memset without value-initialization leaves garbage data:
std::array<char, 256> hostname;
```

Good:

```cpp
// Explicit value-initialization zero-initializes primitive elements:
const size_t buffer_size = 50;
std::array<uint8_t, buffer_size> buffer = {};

// Modern C++ value-initialization replacing memset:
std::array<char, 256> hostname = {};
```
